
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#endif

#include <mbedtls/mbedtls_config.h>
#include <mbedtls/build_info.h>
#include <mbedtls/platform.h>
#include <psa/crypto.h>
#include <mbedtls/x509.h>
#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/error.h>
#include <mbedtls/debug.h>

#if defined(MBEDTLS_SSL_CACHE_C)
#include <mbedtls/ssl_cache.h>
#endif

#ifndef WIN32
#include <netdb.h>
#include <arpa/inet.h>
#else
//#include <winsock2.h>
//#include <ws2tcpip.h>
#endif

#undef bind
#include <import>

// Implementation example:
Session Session_with_TLS(Session s, TLS tls) {
    mbedtls_ssl_init(&s->ssl);
    mbedtls_net_init(&s->fd);
    mbedtls_ssl_setup(&s->ssl, &tls->conf);
    s->tls = tls;
    return s;
}

Session Session_with_uri(Session s, uri addr) {
    s->tls = TLS(url, addr); 
    return s;
}

bool Session_bind_addr(Session s, uri addr) {
    string s_port = f(string, "%i", addr->port);
    i32 res = mbedtls_net_bind(&s->fd, addr->host->chars, s_port->chars, MBEDTLS_NET_PROTO_TCP);
    if (res != 0) {
        verify(false, "mbedtls_net_bind: fails with %i", res);
        return false;
    }
    return true;
}

bool Session_connect_to(Session s) {
    string host = s->tls->url->host;
    i32    port = s->tls->url->port;
    
    i32 ret = mbedtls_ssl_setup(&s->ssl, &s->tls->conf);
    if (ret != 0) {
        error("mbedtls_ssl_setup failed: %i", ret);
        return false;
    }

    string str_port = f(string, "%i", port ? port : 443);
    ret = mbedtls_ssl_set_hostname(&s->ssl, host->chars);
    if (ret != 0) {
        error("mbedtls_ssl_set_hostname failed: %i", ret);
        return false;
    }
    
    ret = mbedtls_net_connect(&s->fd, host->chars, str_port->chars, MBEDTLS_NET_PROTO_TCP);
    if (ret != 0) {
        error("mbedtls_net_connect failed: %i", ret);
        return false;
    }
    
    mbedtls_ssl_set_bio(&s->ssl, &s->fd, mbedtls_net_send, mbedtls_net_recv, null);
    
    while ((ret = mbedtls_ssl_handshake(&s->ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            error("mbedtls_ssl_handshake failed: %i", ret);
            return false;
        }
    }
    s->connected = true;
    return true;
}

bool Session_close(Session s) {
    i32 ret;
    while ((ret = mbedtls_ssl_close_notify(&s->ssl)) < 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && 
            ret != MBEDTLS_ERR_SSL_WANT_WRITE &&
            ret != MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            verify(false, "mbedtls_ssl_close_notify returned %i", ret);
            return false;
        }
    }
    return true;
}

none Session_set_timeout(Session s, i64 t) {
    s->timeout_ms = t;
}

bool Session_read_sz(Session s, handle v, sz sz) {
    i32 st = 0;
    for (i32 len = sz; len > 0;) {
        i32 rcv = mbedtls_ssl_read(&s->ssl, v + st, len);
        if (rcv <= 0)
            return false;
        len -= rcv;
        st  += rcv;
    }
    return true;
}

sz Session_recv(Session s, handle buf, sz len) {
    sz sz;
    do {
        sz = mbedtls_ssl_read(&s->ssl, buf, len);
        if (sz == MBEDTLS_ERR_SSL_WANT_READ || sz == MBEDTLS_ERR_SSL_WANT_WRITE)
            continue;

        if (sz == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET)
            continue;
        
        break;
    } while(1);
    return sz;
}

sz Session_send(Session s, handle buf, sz len) {
    sz ret;
    while ((ret = mbedtls_ssl_write(&s->ssl, buf, len)) <= 0) {
        if (ret == MBEDTLS_ERR_NET_CONN_RESET)
            return 0;
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
            break;
    }
    return ret;
}

sz Session_send_string(Session s, string v) {
    printf("%s", v->chars);
    if (len(v) && v->chars[len(v) - 1] != '\n')
        printf("\r\n");
    return send(s, v->chars, v->count);
}

vector Session_read_until(Session s, string match, i32 max_len) {
    vector rbytes = new(vector);
    vector_reallocate(rbytes, max_len + 1);

    sz slen = match->count;
    cstr buf = (cstr)vdata(rbytes);
    
    for (;;) {
        vector_push(rbytes, _i8(0)); // extend buffer size here with a null, giving us space to read
        sz sz = rbytes->count;
        verify(sz, "invalid");
        if (!recv(s, &buf[sz - 1], 1))
            return null;
            
        if (sz >= slen && 
            memcmp(&buf[sz - slen], match->chars, slen) == 0)
            break;
            
        if (sz == max_len)
            return null;
    }
    return rbytes;
}

Session Session_accept(TLS tls) {
    Session client = Session(tls);
    
    for (;;) {
        mbedtls_net_init(&client->fd);
        mbedtls_ssl_setup(&client->ssl, &client->tls->conf);

        i32 ret;
        if ((ret = mbedtls_net_accept(&tls->fd, &client->fd, null, 0, null)) != 0) {
            return null;
        }
        mbedtls_ssl_session_reset(&client->ssl);
        
        bool retry = false;
        mbedtls_ssl_set_bio(&client->ssl, &client->fd, 
                           mbedtls_net_send, mbedtls_net_recv, null);
        while ((ret = mbedtls_ssl_handshake(&client->ssl)) != 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && 
                ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                verify(false, "mbedtls_ssl_handshake: %i", ret);
                retry = true;
                break;
            }
        }
        if (!retry)
            break;
    }
    client->connected = true;
    return client;
}


void mbedtls_debug(void *ctx, int level, const char *file, int line, const char *str) {
    ((void) level);
    fprintf((FILE *) ctx, "mbedtls: %s:%04d: %s", file, line, str);
    fflush((FILE *) ctx);
}

none TLS_init(TLS tls) {
    static bool init_done = false;
    if (!init_done) {
        #ifdef _WIN32
        WSADATA wsa_data;
        i32 wsa = WSAStartup(MAKEWORD(2,2), &wsa_data);
        if (wsa != 0) {
            print("(sock) WSAStartup failed: %i", wsa);
            return;
        }
        #endif
        init_done = true;
    }

    mbedtls_net_init(&tls->fd);
    mbedtls_ssl_config_init(&tls->conf);
    mbedtls_x509_crt_init(&tls->srvcert);
    mbedtls_pk_init(&tls->pkey);
    psa_crypto_init();
    //mbedtls_ctr_drbg_init(&tls->ctr_drbg);

    static string pers;
    if (!pers) pers = new(string, "Au-type::net");

    u8 random[32];
    i32 status = psa_generate_random(random, sizeof(random));
    if (status != PSA_SUCCESS) {
        printf("psa_generate_random failed: %d\n", status);
        return;
    }

    string host = tls->url->host;
    i32    ret  = 0;

    if (!tls->is_client) {
        string pub  = f(string, "ssl/%o.crt", host);
        string prv  = f(string, "ssl/%o.key", host);
        ret = mbedtls_x509_crt_parse_file(&tls->srvcert, pub->chars);
        if (ret != 0) {
            print("mbedtls_x509_crt_parse returned %i\n", ret);
            return;
        }
    
        ret = mbedtls_pk_parse_keyfile(&tls->pkey, prv->chars, 0);
        if (ret != 0) {
            print("mbedtls_pk_parse_key returned %i\n", ret);
            return;
        }
    } else {
        path cw = path_cwd();
        string trust = f(string, "ssl/trust.crt");
        int parse_res = mbedtls_x509_crt_parse_file(&tls->srvcert, trust->chars);
        mbedtls_ssl_conf_ca_chain(&tls->conf, &tls->srvcert, NULL);
    }

    if (!tls->is_client) {
        string port = f(string, "%i", tls->url->port ? tls->url->port : 443);
        ret = mbedtls_net_bind(&tls->fd, host->chars, port->chars, MBEDTLS_NET_PROTO_TCP);
        if (ret != 0) {
            print("mbedtls_net_bind returned %i\n", ret);
            return;
        }
    }

    ret = mbedtls_ssl_config_defaults(
        &tls->conf,
        tls->is_client ? MBEDTLS_SSL_IS_CLIENT : MBEDTLS_SSL_IS_SERVER,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT);
    
    if (ret != 0) {
        print("mbedtls_ssl_config_defaults returned %i\n", ret);
        return;
    }

    mbedtls_ssl_conf_dbg(&tls->conf, mbedtls_debug, stdout);
    
    if (!tls->is_client) {
        mbedtls_ssl_conf_ca_chain(&tls->conf, tls->srvcert.next, null);
        ret = mbedtls_ssl_conf_own_cert(&tls->conf, &tls->srvcert, &tls->pkey);
        if (ret != 0) {
            print("mbedtls_ssl_conf_own_cert returned %i\n", ret);
            return;
        }
    } else {
        mbedtls_ssl_conf_ca_chain(&tls->conf, &tls->srvcert, NULL);
        mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    }
}

// copy headers first?
none message_init(message m) {
    if (!m->headers) m->headers = new(map, hsize, 24);

    map headers = m->headers;
    uri query   = m->query;

    string ua = f(string, "User-Agent");
    string ac = f(string, "Accept");
    string al = f(string, "Accept-Language");
    string ae = f(string, "Accept-Encoding");
    string h  = f(string, "Host");

    if (!contains(headers, (Au)ua)) set(headers, (Au)ua, (Au)f(string, "silver"));
    if (!contains(headers, (Au)ac)) set(headers, (Au)ac, (Au)f(string, "text/html,application/xhtml+xml,application/xml;q=0.9,*;q=0.8"));
    if (!contains(headers, (Au)al)) set(headers, (Au)al, (Au)f(string, "en-US,en;q=0.9"));
    if (!contains(headers, (Au)ae)) set(headers, (Au)ae, (Au)f(string, "identity"));
    if (!contains(headers, (Au)h))  set(headers, (Au)h,  (Au)query->host);
}

message message_with_sock(message m, sock sc) {
    m->query = sc->data->tls->url;
    m->headers = map();
    if (read_headers(m, sc)) {
        read_content(m, sc);
        string status = (string)get(m->headers, (Au)string("Status"));
        m->code       = atoi(status->chars);
    }
    return m;
}


message message_with_i32(message m, i32 code) {
    m->code = code;
    return m;
}

message message_with_string(message m, string text) {
    m->content = (Au)hold(text);
    m->code = 200;
    return m;
}

message message_with_path(message m, path p, Au modified_since) {
    verify(exists(p) == Exists_file, "path must exist");
    string content = cast(string, load(p, typeid(string), null));
    m->content = (Au)hold(content);
    string mime = (string)f(string, "text/plain"); // TODO: implement mime_type
    set(m->headers, (Au)string("Content-Type"), (Au)mime);
    m->code = 200;
    return m;
}

message message_with_content(message m, Au content, map headers, uri query) {
    m->query = query;
    m->headers = headers;
    m->content = content;
    m->code = 200;
    return m;
}

web message_method_type(message m) {
    return m->query->mtype;
}


bool message_read_headers(message m, sock sc) {
    i32 line = 0;
    for (;;) {
        vector rbytes = read_until(sc, string("\r\n"), 8192);
        sz sz = len(rbytes);
        if (sz == 0)
            return false;
        
        if (sz == 2)
            break;

        string raw = string(chars, (cstr)vdata(rbytes), ref_length, sz - 2);

        if (line++ == 0) {
            m->summary = hold(raw);
            array sp = split(raw, " ");
            // handle response or request line
            if (len(raw) >= 12 && len(sp) >= 3) {
                set(m->headers, (Au)string("Status"), get(sp, 1));
            }
        } else {
            int    sep = index_of(raw, ":");
            string k   = mid(raw, 0, sep);
            string v   = trim(mid(raw, sep + 1, len(raw) - sep - 1));
            set(m->headers, (Au)k, (Au)v);
        }
    }
    return true;
}

bool message_read_content(message m, sock sc) {
    string te = string("Transfer-Encoding");
    string cl = string("Content-Length");
    string ce = string("Content-Encoding");
    
    string encoding = contains(m->headers, (Au)te) ?
        (string)get(m->headers, (Au)ce) : null;
    i32 clen = -1;

    Au o = get(m->headers, (Au)cl);
    if (o) {
        string v = instanceof(o, string);
        if (v) {
            clen = atoi(v->chars);
        } else {
            print("unsupported len format: %s", isa(o)->ident);
        }
    }
    
    string te_s = (string)get(m->headers, (Au)te);
    bool chunked = encoding && te_s && eq(te_s, "chunked");
    num content_len = clen;
    num rlen = 0;
    const num r_max = 1024;
    bool error = false;
    num iter = 0;
    vector v_data = new(vector);

    verify(!(clen >= 0 && chunked), "invalid transfer encoding");

    if (!chunked && clen <= 0) {
        m->content = null;
        return true;
    } else if (!(!chunked && clen == 0)) {
        do {
            if (chunked) {
                if (iter++ > 0) {
                    char crlf[2];
                    if (!read_sz(sc, crlf, 2) || memcmp(crlf, "\r\n", 2) != 0) {
                        error = true;
                        break;
                    }
                }
                vector rbytes = read_until(sc, string("\r\n"), 64);
                if (!rbytes) {
                    error = true;
                    break;
                }
                
                // Parse hex length
                content_len = strtol((cstr)vdata(rbytes), null, 16);
                if (content_len == 0)
                    break;
            }

            bool sff = content_len == -1;
            for (num rcv = 0; sff || rcv < content_len; rcv += rlen) {
                num rx = min(r_max, content_len - rcv);
                char buf[r_max];
                rlen = recv(sc, buf, rx);
                
                if (rlen > 0)
                    concat(v_data, (ARef)buf, rlen);
                else if (rlen < 0) {
                    error = !sff;
                    break;
                } else if (rlen == 0) {
                    error = true;
                    break;
                }
            }
        } while (!error && chunked && content_len != 0);
    }

    if (!error) {
        string ctype = (string)get(m->headers, (Au)string("Content-Type"));

        if (ctype && starts_with(ctype, "application/json")) {
            string js = new(string, chars, (cstr)vdata(v_data), ref_length, len(v_data));
            m->content = hold((Au)Au_parse((Au_t)typeid(map), (cstr)js->chars, (ctx)null));
        } else if (ctype && starts_with(ctype, "text/")) {
            m->content = hold((Au)new(string, chars, (cstr)vdata(v_data), ref_length, len(v_data)));
        } else {
            verify(len(v_data) == 0, "unsupported content type");
            m->content = null;
        }
    }

    return !error;
}


/// query/request construction
message message_query(uri server, map headers, Au content) {
    message m;
    m->query   = uri(
        mtype,web_Get, proto,server->proto, host,server->host,
        port,server->port, query,server->query,
        resource,server->resource, args,server->args,
        version,server->version);
    m->headers = headers;
    m->content = content;
    return m;
}

/// response construction, uri is not needed
message message_response(uri query, i32 code, Au content, map headers) {
    message r;
    r->query    = uri(
        mtype,web_Response, proto,query->proto, host,query->host,
        port,query->port, query,query->query,
        resource,query->resource, args,query->args,
        version,query->version);
    r->code     = code;
    r->headers  = headers;
    r->content  = content;
    return r;
}

symbol code_symbol(i32 code) {
    static map symbols = null;
    if (!symbols) {
        symbols = new(map);
        set(symbols, i(200), (Au)string("OK"));
        set(symbols, i(201), (Au)string("Created"));
        set(symbols, i(202), (Au)string("Accepted"));
        set(symbols, i(203), (Au)string("Non-Authoritative Information"));
        set(symbols, i(204), (Au)string("No Content"));
        set(symbols, i(205), (Au)string("Reset Content"));
        set(symbols, i(206), (Au)string("Partial Content"));
        set(symbols, i(300), (Au)string("Multiple Choices"));
        set(symbols, i(301), (Au)string("Moved Permanently"));
        set(symbols, i(302), (Au)string("Found"));
        set(symbols, i(303), (Au)string("See Other"));
        set(symbols, i(304), (Au)string("Not Modified"));
        set(symbols, i(307), (Au)string("Temporary Redirect"));
        set(symbols, i(308), (Au)string("Permanent Redirect"));
        set(symbols, i(400), (Au)string("Bad Request"));
        set(symbols, i(402), (Au)string("Payment Required"));
        set(symbols, i(403), (Au)string("Forbidden"));
        set(symbols, i(404), (Au)string("Not Found"));
        set(symbols, i(500), (Au)string("Internal Server Error"));
        set(symbols, i(0),   (Au)string("Unknown"));
    }
    string s_code = (string)get(symbols, i(code));
    string result = s_code ? s_code : (string)get(symbols, i(0));
    return result->chars;
}

bool message_cast_bool(message m) {
    return m->query &&
           ((m->code >= 200 && m->code < 300) ||
            (m->code == 0 && (m->content || len(m->headers) > 0)));
}

string A_cast_string(Au);

string message_text(message m) {
    return cast(string, m->content);
}

map message_cookies(message m) {
    string cookies = (string)get(m->headers, (Au)string("Set-Cookie"));
    if (!cookies)
        return new(map);

    string decoded = uri_decode(cookies);
    array  parts   = split(decoded, ",");
    string all     = (string)get(parts, 0);
    array  pairs   = split(all, ";");
    map    result  = new(map);

    each(pairs, string, pair) {
        array kv = split(pair, "=");
        if (len(kv) < 2)
            continue;
            
        string key = (string)get(kv, 0);
        string val = (string)get(kv, 1);
        set(result, (Au)key, (Au)val);
    }

    return result;
}


bool message_write_status(message m, sock sc) {
    string status = string("Status");
    i32 code = 0;
    Au s = get(m->headers, (Au)status);
    if (s) {
        Au_t t = isa(s);
        int test = 1;
        test++;
        code = *(i32*)s;
    } else if (m->code)
        code = m->code;
    else
        code = 200;
    return send_object(sc, (Au)f(string, "HTTP/1.1 %i %s\r\n", code, code_symbol(code)));
}


bool message_write_headers(message m, sock sc) {
    pairs(m->headers, ii) {
        string k = cast(string, ii->key);
        string v = cast(string, ii->value);
        if (strcmp(k->chars, "Status") == 0 || !v)
            continue;
        if (!send_object(sc, (Au)f(string, "%o: %o\r\n", k, v)))
            return false;
    }
    return send_bytes(sc, "\r\n", 2);
}


string encode_fields(map fields) {
    if (!fields) 
        return string("");

    string post = new(string, alloc, 1024);
    bool first = true;

    pairs(fields, i) {
        string k = cast(string, i->key);
        string v = cast(string, i->value);
        
        if (!first) {
            append(post, "&");
        }
        string encoded = f(string, "%s=%s", 
            uri_encode(k)->chars, 
            uri_encode(v)->chars);
        concat(post, encoded);
        first = false;
    }
    
    return post;
}

bool message_write(message m, sock sc, bool last_message) {
    i32 ic = m->code;
    string conn = last_message ? string("close") : string("keep-alive");
    if (ic > 0) {
        symbol s = code_symbol(ic);
        verify(s, "invalid status code");
        string header = f(string, "HTTP/1.1 %i %s\r\n", ic, s);
        if (!send_object(sc, (Au)header))
            return false;
    } else {
        // send uri type
        string method = e_str(web, m->query->mtype);
        string header = f(string, "%o %o HTTP/1.1\r\n", ucase(method), m->query->query);
        if (!send_object(sc, (Au)header))
            return false;
    }

    if (m->content) {
        Au_t ct = isa(m->content);
        
        if (ct && !contains(m->headers, (Au)string("Content-Type")))
            set(m->headers, (Au)string("Content-Type"), (Au)string("application/json"));
        
        set(m->headers, (Au)string("Connection"), (Au)conn);
        
        string headers_ct = (string)get(m->headers, (Au)string("Content-Type"));
        if (ct == typeid(map)) {
            string post = json(m->content);
            set(m->headers, (Au)string("Content-Length"), (Au)_i64(len(post)));
            //set(m->headers, string("Content-Type"),   string("application/json"));
            write_headers(m, sc);
            return send_object(sc, (Au)post);
        } else if (ct == typeid(u8)) {
            num byte_count = header(m->content)->count;
            set(m->headers, (Au)string("Content-Length"), (Au)_i64(byte_count));
            return send_bytes(sc, m->content, byte_count);
        } else {
            verify(ct == typeid(string), "unsupported content type");
            set(m->headers, (Au)string("Content-Length"), (Au)_i64(len((string)m->content)));
            write_headers(m, sc);
            return send_object(sc, m->content);
        }
    }
    
    set(m->headers, (Au)string("Content-Length"), (Au)string("0"));
    set(m->headers, (Au)string("Connection"),     (Au)conn);
    return write_headers(m, sc);
}


string uri_addr(uri u) {
    return dns(u->host);
}

string dns(string hostname) {
    struct addrinfo hints = {0}, *res, *p;
    i32 status;
    char ip[INET6_ADDRSTRLEN];
    
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(hostname->chars, null, &hints, &res);
    if (status != 0) {
        print("DNS lookup failed: %i", status);
        return null;
    }

    string result = null;
    for (p = res; p != null; p = p->ai_next) {
        void* addr;
        if (p->ai_family == AF_INET) {
            struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
            addr = &(ipv4->sin_addr);
        } else {
            struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)p->ai_addr;
            addr = &(ipv6->sin6_addr);
        }

        char* ip_str = (char*)inet_ntop(p->ai_family, addr, ip, sizeof(ip));
        if (ip_str) {
            result = string(ip_str);
            break;
        }
    }

    freeaddrinfo(res);
    return result;
}

Au request(uri url, map args) {
    map     st_headers   = new(map);
    Au       null_content = null;
    map     headers      = contains(args, (Au)string("headers")) ? (map)get (args, (Au)string("headers")) : st_headers;
    Au       content     = contains(args, (Au)string("content")) ? get (args, (Au)string("content")) : null_content;
    web     type         = contains(args, (Au)string("method"))  ?
        e_val(web, ((string)get(args, (Au)string("method")))->chars) : web_Get;
    uri     query        = url;

    query->mtype = type;
    verify(query->mtype != web_undefined, "undefined web method type");

    sock client = sock(query);
    print("(net) request: %o", url);
    if (!connect_to(client))
        return null;

    // Send request line
    string method = e_str(web, query->mtype);
    send_object(client, (Au)f(string, "%o %o HTTP/1.1\r\n", method, query->query));

    // Default headers
    message request = message(content, content, headers, headers, query, query);
    write(request, client, true);

    message response = message(client);
    close(client);

    return (Au)response;
}

uri uri_with_string(uri a, string raw) {
    array sp = split(raw, " ");
    bool has_method = len(sp) > 1;
    string lcase = len(sp) > 0 ? (string)get(sp, 0) : null;
    web m = e_val(web, has_method ? lcase->chars : "get");
    string u = (string)get(sp, has_method ? 1 : 0);
    a->mtype = m;

    // find protocol separator
    num iproto = index_of(u, "://");
    verify(iproto >= 0, "expected protocol");

    string p = mid(u, 0, iproto);
    u = mid(u, iproto + 3, len(u) - (iproto + 3));
    num ihost = index_of(u, "/");
    a->proto = e_val(protocol, p->chars);
    a->query = ihost >= 0 ? mid(u, ihost, len(u) - ihost) : string("/");
    string h = ihost >= 0 ? mid(u, 0, ihost) : u;
    num ih = index_of(h, ":");
    u = a->query;
    
    if (ih > 0) {
        a->host = mid(h, 0, ih);
        a->port = atoi(mid(h, ih + 1, len(h) - (ih + 1))->chars);
    } else {
        a->host = h;
        a->port = 0; // looked up by method
    }

    // parse resource and query
    num iq = index_of(u, "?");
    if (iq > 0) {
        a->resource = uri_decode(mid(u, 0, iq));
        string q = uri_decode(mid(u, iq + 1, len(u) - (iq + 1)));
        array all = split(q, "&");
        a->args = new(map);
        
        each(all, string, kv) {
            array sp = split(kv, "=");
            Au     k = get(sp, 0);
            Au     v = len(sp) > 1 ? get(sp, 1) : k;
            set(a->args, k, v);
        }
    } else {
        a->resource = uri_decode(u);
    }

    if (len(sp) >= 3) {
        a->version = (string)get(sp, 2);
    }

    return a;
}

uri uri_with_cstr(uri a, cstr addr) {
    return uri_with_string(a, string(addr));
}


string uri_encode(string s) {
    static string chars;
    if (!chars) chars = string(" -._~:/?#[]@!$&'()*+;%=");
    
    sz len = len(s);
    string v = string(alloc, len * 2);
    
    for (sz i = 0; i < len; i++) {
        char c = s->chars[i];
        bool a = ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'));
        if (!a) {
            char ch[2] = { c, 0 };
            a = index_of(chars, ch) != -1;
        }
            
        if (!a) {
            append(v, "%");
            char hex[3];
            snprintf(hex, sizeof(hex), "%02x", (u8)c);
            append(v, hex);
        } else {
            char ch[2] = { c, 0 };
            append(v, ch);
        }
        
        if (c == '%')
            append(v, "%");
    }
    
    return v;
}

string uri_decode(string e) {
    num sz = len(e);
    string v = string();
    num i = 0;

    while (i < sz) {
        char c0 = e->chars[i];
        char cstr[2] = { c0, 0 };
        if (c0 == '%') {
            if (i >= sz - 1)
                break;
                
            char c1 = e->chars[i + 1];
            if (c1 == '%') {
                append(v, "%");
            } else {
                if (i >= sz - 2)
                    break;
                    
                char c2 = e->chars[i + 2];
                char hex[3] = {c1, c2, 0};
                u8 val;
                sscanf(hex, "%hhx", &val);
                char vstr[2] = { val, 0 };
                append(v, vstr);
                i += 2;
            }
        } else {
            append(v, (c0 == '+') ? " " : cstr);
        }
        i++;
    }
    
    return string(chars, v->chars, ref_length, len(v));
}

// The handler function signature would be:
Au handle_client(Au target, Au client_sock, Au context) {
    sock s = (sock)client_sock;
    // Handle the client socket
    return _bool(true);
}

Au sock_listen(uri url, subprocedure handler) {
    TLS tls = TLS(url, url);
    
    for (;;) {
        sock client = sock_accept(tls);
        if (!client)
            break;

        // handler function will receive:
        // - target from handler->target
        // - client as the data arg 
        // - context from handler->ctx
        Au result = invoke(handler, (Au)client);
        if (!cast(bool, result))
            break;
    }
    return (Au)tls;
}

sock sock_with_TLS(sock s, TLS tls) {
    //s->url  = hold(tls->url);
    s->data = Session(tls);
    return s;
}

sock sock_with_uri(sock s, uri addr) {
    TLS tls = TLS(url, addr, is_client, true);
    sock sc = sock_with_TLS(s, tls);
    return sc;
}

sock sock_with_cstr(sock a, cstr addr) {
    uri u = uri(addr);
    return sock_with_uri(a, u);
}

bool sock_bind_addr(sock s, uri addr) {
    return Session_bind_addr(s->data, addr);
}

bool sock_connect_to(sock s) {
    return Session_connect_to(s->data);
}

bool sock_close(sock s) {
    return Session_close(s->data);
}

none sock_set_timeout(sock s, i64 t) {
    Session_set_timeout(s->data, t);
}

bool sock_read_sz(sock s, handle v, sz sz) {
    return Session_read_sz(s->data, v, sz);
}

sz sock_recv(sock s, handle buf, sz len) {
    return Session_recv(s->data, buf, len);
}

sz sock_send_bytes(sock s, handle buf, sz len) {
    printf("%s", (cstr)buf);
    return Session_send(s->data, buf, len);
}

sz sock_send_object(sock s, Au v) {
    string str = cast(string, v);
    return Session_send_string(s->data, str);
}

vector sock_read_until(sock s, string match, i32 max_len) {
    return Session_read_until(s->data, match, max_len);
}

sock sock_accept(TLS tls) {
    Session s = Session_accept(tls);
    return s ? sock(s->tls) : null;
}

bool sock_cast_bool(sock s) {
    return s->data->connected;
}

bool sock_read(sock s, handle buf, sz len) {
    sz actual = recv(s, buf, len);
    return actual == len;
}

// For JSON requests, success/failure handlers would have signatures like:
Au on_success(Au target, Au response_data, Au context) {
    // Handle successful JSON response
    return response_data;
}

Au on_failure(Au target, Au error_data, Au context) {
    // Handle failure
    return null;
}

Au json_request(uri addr, map args, map headers, subprocedure success_handler, subprocedure failure_handler) {
    Au response = request(addr, headers);
    
    if (!response) {
        return invoke(failure_handler, null);
    }

    if (isa(response) == typeid(map)) {
        return invoke(success_handler, response);
    } else {
        return invoke(failure_handler, response);
    }
}

define_class(uri,       Au)
define_class(Session,   Au)
define_class(TLS,       Au)
define_class(sock,      Au)
define_class(message,   Au)

define_enum(web)
define_enum(protocol)
