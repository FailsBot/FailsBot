#include "botapi/bot_easy_api.h"
#include <string>
#include "botapi/writefn_data.h"
#include "json.hpp"
#include "cfg/botkey.h"

/// @brief Thin RAII-wrapper for CURL escaped strings
class CurlAutoEscape
{
	char *t;
public:
	CurlAutoEscape(CURL *c, const char *s, size_t len = 0) {
		t = curl_easy_escape(c, s, len);
	}

	operator char *() {
		return t;
	}

	~CurlAutoEscape() {
		curl_free(t);
	}
};

/// @brief Returns string with vzhuh-cat and your message with optional
///        prefix.
/// @param msg    Message to vzhuh.
/// @param prefix It will be added before msg.
/// @param c      CURL handle, needed for escaping, if null, then assume
///               that the msg and prefix are escaped.
std::string make_vzhuh_str(const char *msg, const char *prefix, CURL *c = 0)
{
	static const char *cat = "%60%60%60%0A%20%E2%88%A7%EF%BC%BF%E2%88%A7%0A%28%20%EF%BD%A5%CF%89%EF%BD%A5%EF%BD%A1%29%E3%81%A4%E2%94%81%E2%98%86%E3%83%BB%2A%E3%80%82%0A%E2%8A%82%E3%80%80%20%E3%83%8E%20%E3%80%80%E3%80%80%E3%80%80%E3%83%BB%E3%82%9C%2B.%0A%E3%81%97%E3%83%BC%EF%BC%AA%E3%80%80%E3%80%80%E3%80%80%C2%B0%E3%80%82%2B%20%2A%C2%B4%C2%A8%29%0A%E3%80%80%E3%80%80%E3%80%80%E3%80%80%E3%80%80%E3%80%80%E3%80%80%E3%80%80%E3%80%80.%C2%B7%20%C2%B4%C2%B8.%C2%B7%2A%C2%B4%C2%A8%29%20%C2%B8.%C2%B7%2A%C2%A8%29%0A%E3%80%80%E3%80%80%E3%80%80%E3%80%80%E3%80%80%E3%80%80%E3%80%80%E3%80%80%E3%80%80%E3%80%80%28%C2%B8.%C2%B7%C2%B4%20%28%C2%B8.%C2%B7%27%2A%20%E2%98%86%60%60%60";
	std::string s = cat; // msg;
	const char *esc_msg = msg;
	const char *esc_pref = prefix;
	if (c) {
		esc_msg = msg ? curl_easy_escape(c, msg, 0) : "";
		esc_pref = prefix ? curl_easy_escape(c, prefix, 0) : "";
	}
	if (prefix) {
		s = s + esc_pref;
	}
	if (msg) {
		s += esc_msg;
	}
	if (c) {
		if (msg) {
			curl_free((char*)esc_msg);
		}
		if (prefix) {
			curl_free((char*)esc_pref);
		}
	}
	return s;
}

void handleUpdate(CURL * c, const nlohmann::json &res,
	bool quit, size_t &updateOffset)
{
	TgInteger updId = res["update_id"].get<TgInteger>();

	if (res.find("message") == res.end()) {
		updateOffset = updId + 1;
		return;
	}

	auto &msg = res["message"];
	if (msg.count("text") != 1) {
		updateOffset = updId + 1;
		return;
	}
	auto &text = msg["text"];
	if (msg.count("chat") != 1) {
		updateOffset = updId + 1;
		return;
	}
	auto &chat = msg["chat"];
	TgInteger chatId = chat["id"].is_number() ? chat["id"].get<TgInteger>() : 0;
	std::string msgText = text.is_string() ? text.get<std::string>() : "";
	if ((msgText.compare("/vzhuh") == 0 || msgText.compare("/vzhuh" "@" BOT_NAME) == 0) && chatId != 0) {
		easy_perform_sendMessage(c, chatId, make_vzhuh_str(0, 0).c_str(), TgMessageParseMode::TgMessageParse_Markdown, 0, 0, 0);
	}

	updateOffset = updId + 1;
}

void handleUpdates(CURL * c, nlohmann::json &upd, bool &quit, size_t &updateOffset)
{
	auto &r = upd["result"];
	if (!r.is_array()) {
		quit = true;
		return;
	}

	for (auto &res : r) {
		handleUpdate(c, res, quit, updateOffset);
	}

}

CURL *bot_network_init();

int main(int argc, char *argv[])
{
	using nlohmann::json;
	size_t sleep_time = 10;
	size_t upd_id = 0;
	writefn_data d;
	json upd;
	bool quit = false;
	CURL *c = bot_network_init();

	do {
		writefn_data_init(d);
		if (easy_perform_getUpdates(c, &d, sleep_time, upd_id) != CURLE_OK) {
			fprintf(stderr, "Bot network error.\n");
			break;
		}
		if (easy_get_http_code(c) != 200) {
			printf("%s\n", d.ptr);
			break;
		}
		printf("%s\n", d.ptr);
		upd = json::parse(d.ptr);
		if (upd["ok"].is_null() || !upd["ok"].is_boolean() || !(upd["ok"].get<bool>())) {
			fprintf(stderr, "Telegram server returns non-ok result: %s\n", d.ptr);
			break;
		}
		handleUpdates(c, upd, quit, upd_id);
		writefn_data_free(d);
	} while (!quit);
	if (!quit) {
		writefn_data_free(d);
	}
	curl_easy_cleanup(c);
	return 0;
}
