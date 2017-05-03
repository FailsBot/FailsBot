// #include "include/to_string.h"
#include "json/json.hpp"
#include <curl/curl.h>
#include "curl/easy.h"
#include <stdint.h>
#include <sstream>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

typedef int64_t TgInteger; 

typedef struct TgLocation {
	float latitude;
	float longitude;
} TgLocation;

enum TgMessageParseMode {
	TgMessageParse_Normal,
	TgMessageParse_Markdown,
	TgMessageParse_Html,
};

using nlohmann::json;

struct const_string {
	const char *str;
	size_t len;
};

#define COUNTOF(arr) (sizeof(arr) / sizeof(arr[0]))

#define MAKE_CONST_STR(str) {(str), COUNTOF(str) - 1}

// pretty macros for cute terminal coloring
#define RED(s)      "\x1b[1;31m" s "\x1b[0m"
#define GREEN(s)    "\x1b[1;32m" s "\x1b[0m"
#define MAGENTA(s)  "\x1b[1;35m" s "\x1b[0m"
#define YELLOW(s)   "\x1b[1;33m" s "\x1b[0m"
#define BLUE(s)     "\x1b[1;34m" s "\x1b[0m"

#define BOT_URL "https://api.telegram.org/bot<copy_token_here>/"

const char *boturl = BOT_URL;

//
// resizable array.
//

struct writefn_data {
	char *ptr;
	size_t sz;
};

void writefn_data_init(writefn_data &d)
{
	d.ptr = (char*)malloc(1);
	d.sz = 0;
}

bool writefn_data_resize(writefn_data &d, size_t add_sz)
{
	char *old = d.ptr;
	d.ptr = (char*)realloc(d.ptr, d.sz + add_sz);
	if (!d.ptr) {
		d.ptr = old;
		return false;
	}
	d.sz += add_sz;
	return true;
}

bool writefn_data_append(writefn_data &d, const char *data, size_t sz)
{
	size_t old_sz = d.sz;

	if (!sz) {
		return true;
	}
	
	if (!writefn_data_resize(d, sz)) {
		return false;
	}
	memcpy(d.ptr + old_sz, data, sz);
	return true;
}

/// @brief free()s ptr and zeroes struct info.
void writefn_data_free(writefn_data &d)
{
	free(d.ptr);
	d.ptr = 0;
	d.sz = 0;
}

// Bot global context
struct global_writefunc_cb {
	std::stringstream s;
	std::string str;
	size_t slp;
	json js;
	TgInteger upd;
	TgInteger restartChatId;
	TgInteger malbolgeLastMsgId;
	bool quit;
	bool restart;
	bool lastInline;
	bool sendToTgof;
	const char *compile;
	const char *sh;
	const char *battery;
	writefn_data data;
} my;

//
// Bot easy API
//

int easy_get_http_code(CURL *c)
{
	int st = 0;
	int st2 = curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &st);
	return st2 == CURLE_OK ? st : - st2;
}

/// @brief Prints HTTP code and responce. 
void easy_print_http_code(CURL *c, writefn_data *d = 0)
{
	int status = 0;
	curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
	printf("HTTP status: %s(%d)\n",
		(status < 400) ? GREEN("OK") : RED("FAILED"), status);
	if (!d) {
		return;
	}
	printf("%s\n", d->ptr);
}

int easy_perform_commandstr(CURL *c, const char *url, writefn_data *data, 
	bool print_result = true)
{
	assert(data);
	std::cout << "Perform command " << url << std::endl;
	curl_easy_setopt(c, CURLOPT_URL, url);
	// if (data) {
		curl_easy_setopt(c, CURLOPT_WRITEDATA, data);
	// }
	int result = curl_easy_perform(c);
	if (print_result) {
		easy_print_http_code(c);
	}
	return result;
}

#include <fstream>

int easy_perform_getUpdates(CURL *c, writefn_data *d, size_t poll_time = 0)
{
	std::string s =  BOT_URL "getUpdates";
	bool que = false;
	if (my.upd != 0) {
		que = true;
		s += "?offset=" + std::to_string(my.upd);
	}
	if (poll_time != 0) {
		s += (que ? "&timeout=": "?timeout=") + std::to_string(poll_time);
	}
	int r = easy_perform_commandstr(c, s.c_str(), d);
	return r;
}

int easy_perform_getUpdates_auto(CURL *c)
{
	writefn_data d;
	writefn_data_init(d);
	int r = easy_perform_getUpdates(c, &d);
	easy_print_http_code(c);
	writefn_data_free(d);
	return r;
}

int easy_perform_sendMessage(CURL *c, const char *chat_id, 
	const char *msg, TgMessageParseMode mode, TgInteger reply_id,
	const char *additional = 0, writefn_data *d2 = 0)
{
	static const char *modes[] = {
		"Markdown",
		"HTML",
	};

	std::string query = BOT_URL "sendMessage";
	query += "?chat_id=" + std::string(chat_id) + "&text=" + std::string(msg);
	writefn_data d;
	writefn_data_init(d);
	if (mode != TgMessageParse_Normal) {
		query += "&parse_mode=";
		query += modes[mode - 1];
	}
	if (additional) {
		query += additional;
	}
	if (reply_id) {
		query += "&reply_to_message_id=" + std::to_string(reply_id);
	}
	int r = easy_perform_commandstr(c, query.c_str(), &d);
	if (d2 == 0) {
		writefn_data_free(d);
	} else {
		*d2 = d;
	}
	return r;
}

int easy_perform_sendMessage(CURL *c, TgInteger chat_id, 
	const char *msg, TgMessageParseMode mode, TgInteger reply_id,
	const char *additional = 0, writefn_data *d2 = 0)
{
	return easy_perform_sendMessage(c, std::to_string(chat_id).c_str(),
			msg, mode, reply_id, additional, d2);
}

int easy_perform_sendMessage_s(CURL *c, const char *chat_id,
		const char *msg, bool markdown,
		const char *additional = 0, TgInteger reply_id = 0, 
		writefn_data *d2 = 0)
{
	return easy_perform_sendMessage(c, chat_id, msg,
			markdown ? TgMessageParse_Markdown :
			TgMessageParse_Normal, reply_id, additional, d2);
}

int easy_perform_sendMessage(CURL *c, TgInteger chat_id, const char *msg,
		bool markdown, const char *additional = 0,
		TgInteger reply_id = 0,
		writefn_data *d = 0)
{
	return easy_perform_sendMessage_s(c, std::to_string(chat_id).c_str(),
			msg, markdown, additional, reply_id, d);	
}

int easy_perform_sendChatAction(CURL *c, TgInteger chat_id,
		const char *action)
{
	std::string s = BOT_URL "sendChatAction?chat_id=" +
		std::to_string(chat_id) + "&action=" + 
		std::string(action);
	writefn_data d;
	writefn_data_init(d);
	int res = easy_perform_commandstr(c, s.c_str(), &d);
	printf("%s\n",d.ptr);
	writefn_data_free(d);
	return res;
}

//
// chunked msg
//

bool ishexnum(int c)
{
	return  (c >= '0' && c <= '9') ||
		(c >= 'a' && c <= 'f') ||
		(c >= 'A' && c <= 'F');
}

bool find_percent_enc(char *buf, size_t &cur_off, size_t max_offset)
{
	size_t old_off = cur_off;
	while (ishexnum(buf[cur_off])) {
		if (cur_off == 0) {
			cur_off = old_off;
			return false;
		}
		cur_off--;
	}
	if (buf[cur_off] == '%') {
		return true;
	}
	return false;
}

typedef bool (*chunk_perform_callback)(const char *buf, size_t sz,
		void *param);

bool easy_perform_chunked_message(char *buf, size_t bufsz,
		const size_t MAX_MSG_SIZE, chunk_perform_callback cb,
		void *param)
{
	char *chunk = buf;
	char old;
	size_t counter = MAX_MSG_SIZE;
	if (bufsz < counter) {
		cb(buf, bufsz, param);
		return true;
	}
	for (size_t i = 0; i < bufsz; i += counter, bufsz -= counter) {
		counter = MAX_MSG_SIZE;
		
		if (!find_percent_enc(chunk, counter, MAX_MSG_SIZE)) {
			// assert(0);
		}
		old = chunk[counter];
		chunk [counter] = '\0';
		cb(chunk, bufsz, param);
		chunk [counter] = old;
		chunk += counter;
	}
	cb(chunk, bufsz, param);
	return true;
}

int easy_perform_sendEscapedLongMessage(CURL *curl, TgInteger chatId,
		const char *msg, size_t length,
		TgMessageParseMode parseMode, TgInteger replyId,
		const char *additional, writefn_data *d)
{
	char *a = curl_easy_escape(curl, msg, length);
	struct sndmsg_chunk_ctx {
		CURL *c;
		TgInteger chat;
		const char *additional;
		static bool snd(const char *buf,
				size_t sz, void *p){
			sndmsg_chunk_ctx *c = 
				(sndmsg_chunk_ctx*)p;
		easy_perform_sendMessage(c->c, c->chat, buf, 0,
				c->additional);
			return true;
		} 
	} c = {
		curl,
		chatId,
		additional,
	};
	int result = easy_perform_chunked_message(a, strlen(a),
		4096, c.snd, &c);
	curl_free(a);
	return result;
}

int easy_perform_sendSticker(CURL *c, TgInteger chat_id, const char *sticker_id,
	TgInteger reply_id = 0,	const char *additional = 0)
{
	writefn_data d;
	writefn_data_init(d);
	std::string query = BOT_URL "sendSticker";
	query += "?chat_id=" + std::to_string(chat_id) + "&sticker=";
	query += sticker_id;
	if (additional) {
		query += additional;
	}
	if (reply_id) {
		query += "&reply_to_message_id=" + std::to_string(reply_id);
	}
	int r = easy_perform_commandstr(c, query.c_str(), &d); 
	writefn_data_free(d);
	return r;
}

int easy_perform_sendLocation(CURL *c, TgInteger chat_id,
		const TgLocation &loc,
		TgInteger reply_id = 0, writefn_data *d = 0)
{
	std::string s = BOT_URL + std::string("sendLocation?latitude=") + 
		std::to_string(loc.latitude) + "&longitude=" +
		std::to_string(loc.longitude) + "&chat_id=" +
		std::to_string(chat_id);
	if (reply_id) {
		s += "&reply_to_message_id=" + std::to_string(reply_id);
	}
	return easy_perform_commandstr(c, s.c_str(), d);
}

int easy_perform_sendVenue(CURL *c, TgInteger chat_id,
		const TgLocation &loc,
		const char *title, const char *address,
		TgInteger reply_id = 0, writefn_data *d = 0)
{
	std::string s = BOT_URL "sendVenue";
	s += std::string("?chat_id=") + std::to_string(chat_id);
	char *t = curl_easy_escape(c, title, 0);
	char *a = curl_easy_escape(c, address, 0);
	s += std::string("&title=") + std::string(t) + "&address="
		+ std::string(a) + "&latitude="
		+ std::to_string(loc.latitude) + "&longitude="
		+ std::to_string(loc.longitude);
	curl_free(t);
	curl_free(a);
	if (reply_id) {
		s += "&reply_to_message_id=" + std::to_string(reply_id);
	}
	return easy_perform_commandstr(c, s.c_str(), d);
}

size_t json_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	assert(userdata != 0);
	size_t act = size * nmemb;
	writefn_data *data = (writefn_data *)userdata;
	
	if (!writefn_data_append(*data, ptr, act + 1)) {
		std::cout << RED("out of mem(can't realloc)\n");
		return 0;
	}
	data->ptr[data->sz - 1]='\0';
	data->sz--;
	return act;
}

int easy_perform_forwardMessage(CURL *c, TgInteger chatId,
		TgInteger messageId, const char *channel)
{
	writefn_data data;
	writefn_data_init(data);

	std::string s = BOT_URL "forwardMessage?from_chat_id="
		"@" + std::string(channel) + "&chat_id="
		+ std::to_string(chatId)
		+ "&message_id=" + std::to_string(messageId);
	int res = easy_perform_commandstr(c, s.c_str(), &data);
	printf("Result: %s", data.ptr);
	writefn_data_free(data);
	if (res != CURLE_OK) {
		return -res;
	}
	return easy_get_http_code(c);
}

void parse_res_getupdates(CURL *c, json &js) 
{
	bool h = false;
	for (auto iter : js) {
		h = 1;
		parse_res_message(c, iter);
	}
	if (h == 1) {
		my.slp = 2;
	} else {
		my.slp += 4;
	}
}

void parse_cb(CURL *curl)
{
	assert(my.js.is_object());
	assert(my.js["ok"].is_boolean() && my.js["ok"].get<bool>() == true);

	assert(my.js["result"].is_array());

	auto &arr = my.js["result"];
	parse_res_getupdates(curl, arr);
		
}

int main(int argc, char *argv[])
{
	writefn_data_init(my.data);
	CURL *curl = curl_easy_init();
	CURLcode ret;
	if (!curl) {
		return 1;
	}
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, json_cb);

	time_t t;
	if (argc > 1) {
		on_restart(curl);
	}
	do {
		t = time(0);
		// get updates
		ret = (CURLcode) easy_perform_getUpdates(curl, &my.data, my.slp);
		if (ret != CURLE_OK) {
			fprintf(stderr, RED("Bot's curl request failed: %s"),
			 curl_easy_strerror(ret));
		
			curl_easy_cleanup(curl);
			return 1;
		} else {
			try {
				std::cout << MAGENTA("parsing mystr: ") << my.data.ptr << std::endl;
				my.js = json::parse(my.data.ptr);
			} catch(...) {
				curl_easy_cleanup(curl);
				fprintf(stderr, RED("Exception while parsing"));
				std::terminate();
			}
			my.str = "";
			writefn_data_free(my.data);
			parse_cb(curl);
			printf("%s", asctime(localtime(&t)));
			if (my.lastInline) {
				my.lastInline = 0;
			} else {
			}
		}
	} while(my.quit == 0);
	
	printf (GREEN("Cleaning..."));
	curl_easy_cleanup(curl);
	
	return 0;
}
