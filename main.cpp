#include <thread>
#include <chrono>
#include "cso_config/config.h"
#include "cso_connector/connector.h"

Error callback(const std::string& sender, const Array<uint8_t>& data) {
	// Handle response message
	for (int i = 0; i < data.length(); ++i) {
		printf("%c ", data[i]);
	}
	printf("\n");
	return Error{};
}

void loopSendMessage(const std::string& sender, IConnector* connector) {
	Error err;
	while (true) {
		err = connector->sendMessage(sender, Array<uint8_t>{ "hello" }, true, false);
		if (!err.nil()) {
			printf("%s\n", err.toString().c_str());
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
}

int main() {
	auto connector = Connector::build(
		50, Config::build(
			"project-id",
			"project-token",
			"connection-name",
			"cso-pubkey",
			"cso-address"
		)
	);

	std::thread thread(&loopSendMessage, "connection-name", connector.get());
	thread.detach();

	connector->listen(callback);
	return 0;
}