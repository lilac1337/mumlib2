// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2015-2022 mumlib2 contributors

//stdlib
#include <map>
#include <thread>

//mumlib
#include "mumlib2/exceptions.h"
#include "mumlib2_private/transport.h"
#include "mumble.pb.h"

using namespace std::literals::chrono_literals;

static auto PING_INTERVAL = 4s;
const long CLIENT_VERSION = 0x01020A;
const std::string CLIENT_RELEASE("Mumlib2");
const std::string CLIENT_OS("OS Unknown");
const std::string CLIENT_OS_VERSION("1");

static std::map<MumbleProto::Reject_RejectType, std::string> rejectMessages = {
		{MumbleProto::Reject_RejectType_None,              "no reason provided"},
		{MumbleProto::Reject_RejectType_WrongVersion,      "wrong version"},
		{MumbleProto::Reject_RejectType_InvalidUsername,   "invalid username"},
		{MumbleProto::Reject_RejectType_WrongUserPW,       "wrong user password"},
		{MumbleProto::Reject_RejectType_WrongServerPW,     "wrong server password"},
		{MumbleProto::Reject_RejectType_UsernameInUse,     "username in use"},
		{MumbleProto::Reject_RejectType_ServerFull,        "server full"},
		{MumbleProto::Reject_RejectType_NoCertificate,     "no certificate provided"},
		{MumbleProto::Reject_RejectType_AuthenticatorFail, "authenticator fail"}
};

namespace mumlib2 {
	Transport::Transport(
		std::function<bool(MessageType, uint8_t*, int)> processMessageFunc,
		std::function<bool(AudioPacket&)> processEncodedAudioPacketFunction,
		std::string cert_file,
		std::string privkey_file) :
		logger("mumlib.Transport"),
		processMessageFunction(std::move(processMessageFunc)),
		processEncodedAudioPacketFunction(std::move(processEncodedAudioPacketFunction)),
		udpSocket(ioService),
		sslContext(asio::ssl::context::sslv23),
		sslContextHelper(sslContext, cert_file, privkey_file),
		sslSocket(ioService, sslContext),
		pingTimer(ioService, std::chrono::seconds(PING_INTERVAL)) {

		pingTimer.async_wait(std::bind(&Transport::pingTimerTick, this, std::placeholders::_1));
	}

	Transport::~Transport() {
		//disconnect();
	}

	void Transport::connect(const std::string& host, int port, const std::string& user, const std::string& password) {

		logger.log("Mumlib2::Transport::connect()");

		std::error_code errorCode;

		connectionParams = make_pair(host, port);
		credentials = make_pair(user, password);
		udpActive = false;
		state = ConnectionState::IN_PROGRESS;

		logger.log("Mumlib2::Transport::connect() -> verify mode");
		sslSocket.set_verify_mode(asio::ssl::verify_peer);

		//todo for now it accepts every certificate, move it to callback
		logger.log("Mumlib2::Transport::connect() -> verify verify callback");
		sslSocket.set_verify_callback([](bool preverified, asio::ssl::verify_context& ctx) { return true; });

		logger.log("Mumlib2::Transport::connect() -> trying to connect");

		try {
			logger.log("Mumlib2::Transport::connect() -> udp");
			asio::ip::udp::resolver resolverUdp(ioService);
			asio::ip::udp::resolver::query queryUdp(asio::ip::udp::v4(), host, std::to_string(port));
			udpReceiverEndpoint = *resolverUdp.resolve(queryUdp);
			udpSocket.open(asio::ip::udp::v4());

			std::array<char, 1> send_buf = { 0 };
			udpSocket.send_to(asio::buffer(send_buf), udpReceiverEndpoint);

			doReceiveUdp();

			logger.log("Mumlib2::Transport::connect() -> tcp");
			asio::ip::tcp::resolver resolverTcp(ioService);
			asio::ip::tcp::resolver::query queryTcp(host, std::to_string(port));

			logger.log("Mumlib2::Transport::connect() -> async_connect");
			async_connect(
				sslSocket.lowest_layer(),
				resolverTcp.resolve(queryTcp),
				bind(&Transport::sslConnectHandler, this, std::placeholders::_1));

		}
		catch (std::runtime_error& exp) {
			logger.log("Mumlib2::Transport::connect() -> failed to establish connection", exp.what());
			throwTransportException(std::string("failed to establish connection: ") + exp.what());
		}
	}

	void Transport::disconnect()
	{
		logger.log("Mumlib2::Transport::disconnect()");

		state = ConnectionState::DISCONNECTING;

		ioService.stop();

		if (state != ConnectionState::NOT_CONNECTED) {
			std::error_code errorCode;

			// todo perform different operations for each ConnectionState
			sslSocket.lowest_layer().close(errorCode);

			udpSocket.shutdown(asio::ip::udp::socket::shutdown_both, errorCode);
			udpSocket.close(errorCode);
			if (errorCode) {
				logger.warn("Not ping: UDP socket close returned error: %s.", errorCode.message().c_str());
			}

			state = ConnectionState::NOT_CONNECTED;
		}
	}

	void Transport::sendVersion() {
		logger.log("Mumlib2::Transport::sendVersion()");

		MumbleProto::Version version;

		version.set_version(CLIENT_VERSION);
		version.set_os(CLIENT_OS);
		version.set_release(CLIENT_RELEASE);
		version.set_os_version(CLIENT_OS_VERSION);

		sendControlMessagePrivate(MessageType::VERSION, version);
	}

	void Transport::sendAuthentication(std::optional<const std::vector<std::string>> tokens) {
		logger.log("Mumlib2::Transport::sendAuthentication()");

		MumbleProto::Authenticate authenticate;
		authenticate.set_username(credentials.first);
		authenticate.set_password(credentials.second);
		authenticate.clear_celt_versions();
		authenticate.set_opus(true);
		authenticate.clear_tokens();
		if (tokens.has_value()) {
			for (const auto& token : *tokens) {
				authenticate.add_tokens(token);
			}
		}

		sendControlMessagePrivate(MessageType::AUTHENTICATE, authenticate);
	}

	void Transport::sendSslPing() {
		logger.log("Mumlib2::Transport::sendSslPing()");

		if (ping_state == PingState::PING) {
			logger.warn("Continue sending SSL ping.");
			disconnect();
			return;
		}

		ping_state = PingState::PING;
		MumbleProto::Ping ping;

		ping.set_timestamp(std::time(nullptr));

		sendControlMessagePrivate(MessageType::PING, ping);
	}


	bool Transport::isUdpActive() {
		return udpActive;
	}

	void Transport::doReceiveUdp()
	{
		udpSocket.async_receive_from(
			asio::buffer(udpIncomingBuffer, MUMBLE_UDP_MAXLENGTH),
			udpReceiverEndpoint,
			[this](const std::error_code& ec, size_t bytesTransferred) {
				if (!ec && bytesTransferred > 0) {
					logger.warn("Received UDP packet of %d B.", bytesTransferred);

					if (!cryptState.isValid()) {
						throwTransportException("received UDP packet before: CRYPT SETUP message");
					}
					else {
						lastReceivedUdpPacketTimestamp = std::chrono::system_clock::now();

						if (udpActive == false) {
							udpActive = true;
							logger.warn("UDP is up.");
						}

						uint8_t plainBuffer[1024];
						const int plainBufferLength = static_cast<const int>(bytesTransferred - 4);

						bool success = cryptState.decrypt(
							udpIncomingBuffer, plainBuffer, static_cast<unsigned int>(bytesTransferred));

						if (!success) {
							throwTransportException("UDP packet: decryption failed");
						}

						auto packet = AudioPacket::Decode(plainBuffer, plainBufferLength, 0);
						processEncodedAudioPacketFunction(packet);
					}

					doReceiveUdp();
				}
				else if (ec == asio::error::operation_aborted) {
					std::error_code errorCode;
					logger.warn("UDP receive function cancelled.");
					if (ping_state == PingState::PING) {
						logger.warn("UDP receive function cancelled PONG.");
					}
				}
				else {
					throwTransportException("UDP receive failed: " + ec.message());
				}
			});
	}

	void Transport::sslConnectHandler(const std::error_code& error) {
		if (!error) {
			sslSocket.async_handshake(asio::ssl::stream_base::client,
				std::bind(&Transport::sslHandshakeHandler, this,
					std::placeholders::_1));
		}
		else {
			disconnect();
		}
	}

	void Transport::sslHandshakeHandler(const std::error_code& error)
	{
		std::error_code errorCode = error;
		if (!error) {
			doReceiveSsl();

			sendVersion();
			sendAuthentication({});
		}
		else {
			throwTransportException(std::string("Handshake failed: ") + error.message());
		}
	}

	void Transport::pingTimerTick(const std::error_code& e) {
		if (state == ConnectionState::CONNECTED) {

			sendSslPing();

			using namespace std::chrono;

			logger.warn("pingTimerTick: Sending UDP ping.");
			sendUdpPing();

			if (udpActive) {
				const auto lastUdpReceivedMilliseconds = duration_cast<milliseconds>(system_clock::now() - lastReceivedUdpPacketTimestamp).count();
				if (lastUdpReceivedMilliseconds > PING_INTERVAL.count() * 1000 + 1000) {
					logger.warn("Didn't receive UDP ping in %d ms, falling back to TCP.", lastUdpReceivedMilliseconds);
				}
			}

		}

		if ((state == ConnectionState::NOT_CONNECTED) && (ping_state == PingState::PING)) {
			logger.warn("pingTimerTick disconnect!.");
			disconnect();
		}

		logger.warn("TimerTick!.");
		pingTimer.expires_at(pingTimer.expires_at() + PING_INTERVAL);
		pingTimer.async_wait(std::bind(&Transport::pingTimerTick, this, std::placeholders::_1));
	}

	void Transport::sendUdpAsync(const uint8_t* buff, int length) {
		if (length > MUMBLE_UDP_MAXLENGTH - 4) {
			throwTransportException("maximum allowed: data length is %d" + std::to_string(MUMBLE_UDP_MAXLENGTH - 4));
		}

		const auto bufSize = std::max(MUMBLE_TCP_MAXLENGTH, MUMBLE_UDP_MAXLENGTH);
		auto* encryptedMsgBuff = reinterpret_cast<uint8_t*>(std::malloc(bufSize));
		const int encryptedMsgLength = length + 4;

		cryptState.encrypt(buff, encryptedMsgBuff, static_cast<unsigned int>(length));

		//logger.warn("Sending %d B of data UDP asynchronously.", encryptedMsgLength);

		udpSocket.async_send_to(
			asio::buffer(encryptedMsgBuff, static_cast<size_t>(length + 4)),
			udpReceiverEndpoint,
			[this, encryptedMsgBuff](const std::error_code& ec, size_t bytesTransferred) {
				std::free(encryptedMsgBuff);
				if (!ec && bytesTransferred > 0) {
					//logger.warn("Sent %d B via UDP.", bytesTransferred);
				}
				else {
					throwTransportException("UDP send failed: " + ec.message());
				}
			});
	}

	void Transport::doReceiveSsl() {
		async_read(
			sslSocket,
			asio::buffer(sslIncomingBuffer, MUMBLE_TCP_MAXLENGTH),
			[this](const std::error_code& error, size_t bytesTransferred) -> size_t {
				if (bytesTransferred < 6) {
					// we need the message header to determine the payload length
					return 6 - bytesTransferred;
				}

				const int payloadSize = ntohl(*reinterpret_cast<uint32_t*>(sslIncomingBuffer.data() + 2));
				const int wholeMessageLength = payloadSize + 6;
				size_t remaining = wholeMessageLength - bytesTransferred;
				remaining = std::max(remaining, (size_t)0);

/*				if (wholeMessageLength > MUMBLE_TCP_MAXLENGTH) {
					throwTransportException(
						std::string("message bigger than max allowed size:") + std::to_string(wholeMessageLength) + "/" + std::to_string(MUMBLE_TCP_MAXLENGTH));
                        }*/

				return remaining;
			},
			[this](const std::error_code& ec, size_t bytesTransferred) {
				if (!ec && bytesTransferred > 0) {

					int messageType = ntohs(*reinterpret_cast<uint16_t*>(sslIncomingBuffer.data()));

					//logger.warn("Received %d B of data (%d B payload, type %d).", bytesTransferred,
					//             bytesTransferred - 6, messageType);

					processMessageInternal(
						static_cast<MessageType>(messageType),
						&sslIncomingBuffer[6],
						static_cast<int>(bytesTransferred - 6));

					doReceiveSsl();
				}
				else {
					logger.error("SSL receiver error: %s. Bytes transferred: %d.",
						ec.message().c_str(), bytesTransferred);
					//todo temporarily disable exception throwing until issue #6 is solved
					//throwTransportException("receive failed: " + ec.message());
				}
			});
	}

	void Transport::processMessageInternal(MessageType messageType, uint8_t* buffer, int length) {
		switch (messageType) {

		case MessageType::UDPTUNNEL: {
			auto packet = AudioPacket::Decode(buffer, length, 0);
			processEncodedAudioPacketFunction(packet);
		}
								   break;
		case MessageType::AUTHENTICATE: {
			logger.warn("Authenticate message received after authenticated.");
		}
									  break;
		case MessageType::PING: {
			MumbleProto::Ping ping;
			ping.ParseFromArray(buffer, length);
			std::stringstream log;
			log << "Received ping.";
			if (ping.has_good()) {
				log << " good: " << ping.good();
			}
			if (ping.has_late()) {
				log << " late: " << ping.late();
			}
			if (ping.has_lost()) {
				log << " lost: " << ping.lost();
			}
			if (ping.has_tcp_ping_avg()) {
				log << " TCP avg: " << ping.tcp_ping_avg() << " ms";
			}
			if (ping.has_udp_ping_avg()) {
				log << " UDP avg: " << ping.udp_ping_avg() << " ms";
			}

			//logger.warn(log.str());
			ping_state = PingState::PONG;
		}
							  break;
		case MessageType::REJECT: {
			MumbleProto::Reject reject;
			reject.ParseFromArray(buffer, length);

			std::stringstream errorMesg;
			errorMesg << "failed to authenticate";

			if (reject.has_type()) {
				errorMesg << ": " << rejectMessages.at(reject.type());
			}

			if (reject.has_reason()) {
				errorMesg << ", reason: " << reject.reason();
			}

			throwTransportException(errorMesg.str());
		}
								break;
		case MessageType::SERVERSYNC: {
			state = ConnectionState::CONNECTED;

			logger.warn("SERVERSYNC. Calling external ProcessControlMessageFunction.");

			processMessageFunction(messageType, buffer, length);
		}
									break;
		case MessageType::CRYPTSETUP: {
			MumbleProto::CryptSetup cryptsetup;
			cryptsetup.ParseFromArray(buffer, length);

			if (cryptsetup.client_nonce().length() != AES_BLOCK_SIZE
				|| cryptsetup.server_nonce().length() != AES_BLOCK_SIZE
				|| cryptsetup.key().length() != AES_BLOCK_SIZE) {
				throwTransportException("one of cryptographic: parameters has invalid length");
			}

			cryptState.setKey(
				reinterpret_cast<const unsigned char*>(cryptsetup.key().c_str()),
				reinterpret_cast<const unsigned char*>(cryptsetup.client_nonce().c_str()),
				reinterpret_cast<const unsigned char*>(cryptsetup.server_nonce().c_str()));

			if (!cryptState.isValid()) {
				throwTransportException("crypt setup: data not valid");
			}

			logger.warn("Set up cryptography for UDP transport. Sending UDP ping.");

			sendUdpPing();


		}
									break;
		default: {
			logger.warn("Calling external ProcessControlMessageFunction.");
			processMessageFunction(messageType, buffer, length);
		}
			   break;
		}
	}

	void Transport::sendUdpPing()
	{
		auto packet = AudioPacket::CreatePingPacket(time(nullptr)).Encode();
		sendUdpAsync(packet.data(), packet.size());
	}

	void Transport::sendSsl(uint8_t* buff, int length) {
		if (!buff || !length) {
			return;
		}

		if (length > MUMBLE_TCP_MAXLENGTH) {
			logger.warn("Sending %d B of data via SSL. Maximal allowed data length to receive is %d B.", length, MUMBLE_TCP_MAXLENGTH);
		}

		try {
			write(sslSocket, asio::buffer(buff, static_cast<size_t>(length)));
		}
		catch (std::system_error& err) {
			logger.log("Mumlib2::Transport::sendSsl() -> failed to send packet with error #", err.code());
			disconnect();
			state = ConnectionState::FAILED;
		}
	}

	void Transport::sendSslAsync(uint8_t* buff, int length) {
		if (length > MUMBLE_TCP_MAXLENGTH) {
			logger.warn("Sending %d B of data via SSL. Maximal allowed data length to receive is %d B.", length,
				MUMBLE_TCP_MAXLENGTH);
		}

		const auto bufSize = std::max(MUMBLE_TCP_MAXLENGTH, MUMBLE_UDP_MAXLENGTH);
		auto* asyncBuff = reinterpret_cast<uint8_t*>(std::malloc(bufSize));
		memcpy(asyncBuff, buff, static_cast<size_t>(length));

		//logger.warn("Sending %d B of data asynchronously.", length);

		async_write(
			sslSocket,
			asio::buffer(asyncBuff, static_cast<size_t>(length)),
			[this, asyncBuff](const std::error_code& ec, size_t bytesTransferred) {
				std::free(asyncBuff);
				if (ec || !bytesTransferred) {
					throwTransportException("async SSL send failed: " + ec.message());
				}
			}
		);
	}

	void Transport::sendControlMessage(MessageType type, google::protobuf::Message& message) {
		if (state != ConnectionState::CONNECTED) {
			logger.warn("sendControlMessage: Connection not established.");
			return;
		}
		sendControlMessagePrivate(type, message);
	}

	void Transport::sendControlMessagePrivate(MessageType type, google::protobuf::Message& message) {


		const uint16_t type_network = htons(static_cast<uint16_t>(type));

		const int size = message.ByteSizeLong();
		const uint32_t size_network = htonl((uint32_t)size);

		const int length = sizeof(type_network) + sizeof(size_network) + size;

		uint8_t* buff = (uint8_t*) malloc(length);

		memcpy(buff, &type_network, sizeof(type_network));

		memcpy(buff + sizeof(type_network), &size_network, sizeof(size_network));

		message.SerializeToArray(buff + sizeof(type_network) + sizeof(size_network), size);

		sendSsl(buff, length);
    free(buff);
	}

	void Transport::throwTransportException(std::string message) {
		state = ConnectionState::FAILED;

		throw TransportException(std::move(message));
	}

	void Transport::sendEncodedAudioPacket(const uint8_t* buffer, int length) {
		if (state != ConnectionState::CONNECTED) {
			logger.warn("sendEncodedAudioPacket: Connection not established.");
			return;
		}

		if (udpActive) {
			sendUdpAsync(buffer, length);
		}
		else {
			const uint16_t netUdptunnelType = htons(static_cast<uint16_t>(MessageType::UDPTUNNEL));
			const uint32_t netLength = htonl(static_cast<uint32_t>(length));

			const int packet = sizeof(netUdptunnelType) + sizeof(netLength) + length;

			uint8_t packetBuff[MUMBLE_UDP_MAXLENGTH];

			memcpy(packetBuff, &netUdptunnelType, sizeof(netUdptunnelType));
			memcpy(packetBuff + sizeof(netUdptunnelType), &netLength, sizeof(netLength));
			memcpy(packetBuff + sizeof(netUdptunnelType) + sizeof(netLength), buffer, static_cast<size_t>(length));

			sendSsl(packetBuff, length + sizeof(netUdptunnelType) + sizeof(netLength));
		}
	}
}
