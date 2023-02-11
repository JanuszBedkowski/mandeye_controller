#include "web_server.h"
#include "mongoose.h"
#include <iostream>
#include <sstream>
#include "web_page.h"

void health_server::setStatusHandler(std::function<std::string()> hndl) {
    produce_status = hndl;
}

void health_server::fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        for (const auto &th : trig_handlers) {
            const std::string &trig = th.first;
            const auto &fun = th.second;
            if (mg_http_match_uri(hm, std::string("/trig/" + trig).c_str())) {
                std::string query(hm->query.ptr, hm->query.len);
                std::string ret = "nok";
                if (fun) {
                    ret = fun(query);
                    mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\n", ret.c_str());
                    return;
                } else {
                    mg_http_reply(c, 404, "Access-Control-Allow-Origin: *\n", ret.c_str());
                    return;
                }

            }
        }
        for (const auto &th : data_handlers) {
            const std::string &trig = th.first;
            const auto &fun = th.second;
            if (mg_http_match_uri(hm, std::string("/data/" + trig).c_str())) {
                std::string ret = "nok";
                if (fun) {
                    int code = 200;
                    std::vector<uint8_t> ret = fun();
                    mg_printf(c, "HTTP/1.1 %d %s\r\n%sContent-Length: %d\r\n\r\n", code,
                              "OK", "Access-Control-Allow-Origin: *\n", ret.size());

                    mg_send(c, ret.data(), ret.size());
                    return;
                    //mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\n", ret.c_str());
                } else {
                    mg_http_reply(c, 404, "Access-Control-Allow-Origin: *\n", ret.c_str());
                    return;
                }
            }
        }


        if (mg_http_match_uri(hm, "/json/status")) {
            // Serve REST response
            std::string data = "{}";
            if (produce_status) {
                data = produce_status();
            }
            mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\n", data.c_str());
            return;
        }
        if (mg_http_match_uri(hm, "/jquery.js")) {
            mg_http_reply(c, 200, "", gJQUERYData);
            return;
        }
        if (mg_http_match_uri(hm, "/index")) {
            struct mg_http_serve_opts opts{NULL, NULL, NULL, NULL, NULL};
            opts.extra_headers = "Access-Control-Allow-Origin: *\n";
            mg_http_serve_file(c, hm, "index.htm", &opts);
            return;
        }
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\n", gINDEX_HTMData);


    }
    (void) fn_data;
}


void health_server::setTriggerHandler(std::function<std::string(const std::string &)> hndl, std::string trigger) {
    trig_handlers[trigger] = hndl;
}


void health_server::setDataHandler(std::function<std::vector<uint8_t>()> hndl, std::string trigger) {
    data_handlers[trigger] = hndl;
}

void health_server::server_worker() {
    struct mg_mgr mgr;                            // Event manager
    mg_mgr_init(&mgr);                            // Initialise event manager
    mg_http_listen(&mgr, s_listen_on, fn, NULL);  // Create HTTP listener
    for (;;) mg_mgr_poll(&mgr, 1000);             // Infinite event loop
    mg_mgr_free(&mgr);
}


