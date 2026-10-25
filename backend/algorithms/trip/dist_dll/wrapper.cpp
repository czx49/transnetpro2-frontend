#include <Python.h>
#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <mutex>

// 文件日志，增加时间戳，方便在 Qt 客户端调试
static void log_debug(const std::string& msg) {
    SYSTEMTIME st; GetLocalTime(&st);
    std::ostringstream ts;
    ts << std::setfill('0')
       << std::setw(4) << st.wYear << "-" << std::setw(2) << st.wMonth << "-" << std::setw(2) << st.wDay
       << " " << std::setw(2) << st.wHour << ":" << std::setw(2) << st.wMinute << ":" << std::setw(2) << st.wSecond
       << "." << std::setw(3) << st.wMilliseconds;

    std::string log_path = "trip_model_debug.log";
    do {
        wchar_t dll_path[MAX_PATH];
        HMODULE hm = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                (LPCWSTR)&log_debug, &hm)) {
            break;
        }
        if (!GetModuleFileNameW(hm, dll_path, MAX_PATH)) {
            break;
        }
        std::wstring ws(dll_path);
        size_t pos = ws.find_last_of(L"/\\");
        std::wstring dllDir = (pos == std::wstring::npos) ? std::wstring(L".") : ws.substr(0, pos);
        std::wstring logWs = dllDir + L"\\trip_model_debug.log";

        int len = WideCharToMultiByte(CP_UTF8, 0, logWs.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) {
            break;
        }
        std::string utf8(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, logWs.c_str(), -1, &utf8[0], len, nullptr, nullptr);
        if (!utf8.empty() && utf8.back() == '\0') utf8.pop_back();
        log_path = utf8;
    } while (0);

    std::ofstream ofs(log_path, std::ios::app);
    if (ofs.is_open()) {
        ofs << "[" << ts.str() << "] " << msg << std::endl;
    }
}

static void log_debug_large(const std::string& tag, const std::string& payload, size_t chunk_size = 2000) {
    log_debug(tag + " len=" + std::to_string(payload.size()));
    if (payload.empty()) {
        log_debug(tag + " <empty>");
        return;
    }
    if (chunk_size == 0) chunk_size = 2000;
    size_t total = payload.size();
    size_t idx = 0;
    size_t part = 0;
    while (idx < total) {
        size_t n = (idx + chunk_size <= total) ? chunk_size : (total - idx);
        std::string piece = payload.substr(idx, n);
        log_debug(tag + " part=" + std::to_string(part) + " off=" + std::to_string(idx) + " size=" + std::to_string(n) + ": " + piece);
        idx += n;
        part += 1;
    }
}

static std::wstring get_loaded_module_path(const wchar_t* name) {
    HMODULE h = GetModuleHandleW(name);
    if (!h) return L"";
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(h, path, MAX_PATH);
    if (n == 0) return L"";
    return std::wstring(path);
}

// 全局进度回调指针
static void (*g_progress_cb)(int,double) = nullptr;
static double g_scale_a = 1.0; // percent scale
static double g_scale_b = 0.0; // percent offset
static int g_iter_base = 0;    // iteration offset for ALL mode
static int g_last_iter = 0;    // last emitted iteration

// cache paths for config-based initialization
static std::wstring g_pyhome_ws;
static std::wstring g_dlldir_ws;

// Ensure only one thread enters Python runtime via this DLL at a time
static std::mutex g_call_mutex;

// After embedding initialization, release GIL so later calls from other threads won't deadlock
static bool g_gil_released_after_init = false;

// track AddDllDirectory cookies to rollback after call
static PVOID g_cookie_pyhome = nullptr;
static PVOID g_cookie_pyhome_dlls = nullptr;
static PVOID g_cookie_dlldir = nullptr;
static BOOL (WINAPI *g_RemoveDllDirectory)(PVOID) = nullptr;

static PyObject* py_progress(PyObject*, PyObject* args) {
    int it = 0; double pct = 0.0;
    if (!PyArg_ParseTuple(args, "id", &it, &pct)) {
        Py_RETURN_NONE;
    }
    if (g_progress_cb) {
        double sp = g_scale_a * pct + g_scale_b;
        g_progress_cb(it, sp);
    }
    Py_RETURN_NONE;
}

static double extract_double_field(PyObject* root, const std::vector<const char*>& path, double defv = 0.0) {
    PyObject* cur = root;
    for (size_t i = 0; i < path.size(); ++i) {
        if (!cur || !PyDict_Check(cur)) return defv;
        PyObject* key = PyUnicode_FromString(path[i]);
        if (!key) return defv;
        PyObject* nxt = PyDict_GetItem(cur, key); // borrowed
        Py_DECREF(key);
        if (!nxt) return defv;
        cur = nxt;
    }
    if (PyFloat_Check(cur)) return PyFloat_AsDouble(cur);
    if (PyLong_Check(cur)) return (double)PyLong_AsLong(cur);
    return defv;
}

static PyMethodDef ProgressMethods[] = {
    {"progress", (PyCFunction)py_progress, METH_VARARGS, "C callback bridge"},
    {nullptr, nullptr, 0, nullptr}
};

static PyObject* create_progress_callable() {
    return PyCFunction_NewEx(&ProgressMethods[0], nullptr, nullptr);
}

// 固定缩放版：生成阶段 0..50
static PyObject* py_progress_gen(PyObject*, PyObject* args) {
    int it = 0; double pct = 0.0;
    if (!PyArg_ParseTuple(args, "id", &it, &pct)) {
        Py_RETURN_NONE;
    }
    if (g_progress_cb) {
        // 不再关心内部迭代号，只做线性百分比映射
        // 生成阶段: pct * 0.5
        // 迭代号: it (仅供参考)
        g_progress_cb(it, pct * 0.5);
    }
    Py_RETURN_NONE;
}

static PyMethodDef ProgressMethodsGen[] = {
    {"progress", (PyCFunction)py_progress_gen, METH_VARARGS, "C callback bridge (gen)"},
    {nullptr, nullptr, 0, nullptr}
};

static PyObject* create_progress_callable_gen() {
    return PyCFunction_NewEx(&ProgressMethodsGen[0], nullptr, nullptr);
}

// 固定缩放版：分布阶段 50..100
static PyObject* py_progress_dist(PyObject*, PyObject* args) {
    int it = 0; double pct = 0.0;
    if (!PyArg_ParseTuple(args, "id", &it, &pct)) {
        Py_RETURN_NONE;
    }
    if (g_progress_cb) {
        // 不再关心内部迭代号，只做线性百分比映射
        // 分布阶段: 50 + pct * 0.5
        // 迭代号: 51 + it (仅供参考)
        int display_it = 50 + it;
        g_progress_cb(display_it, 50.0 + pct * 0.5);
    }
    Py_RETURN_NONE;
}

static PyMethodDef ProgressMethodsDist[] = {
    {"progress", (PyCFunction)py_progress_dist, METH_VARARGS, "C callback bridge (dist)"},
    {nullptr, nullptr, 0, nullptr}
};

static PyObject* create_progress_callable_dist() {
    return PyCFunction_NewEx(&ProgressMethodsDist[0], nullptr, nullptr);
}

static void set_out_json(char** out_json, const std::string& s) {
    if (!out_json) return;
    char* buf = (char*) std::malloc(s.size() + 1);
    if (!buf) return;
    std::memcpy(buf, s.c_str(), s.size() + 1);
    *out_json = buf;
}

static std::string json_error(const std::string& msg) {
    std::ostringstream oss;
    oss << "{\"status\":0,\"message\":\"";
    for (char c : msg) {
        if (c == '\"') oss << "\\\"";
        else if (c == '\\') oss << "\\\\";
        else if (c == '\n') oss << "\\n";
        else oss << c;
    }
    oss << "\"}";
    return oss.str();
}

static std::string pyobj_str_utf8(PyObject* obj) {
    if (!obj) return std::string();
    PyObject* s = PyObject_Str(obj);
    if (!s) return std::string();
    const char* c = PyUnicode_AsUTF8(s);
    std::string out = c ? std::string(c) : std::string();
    Py_DECREF(s);
    return out;
}

static std::string fetch_python_error_string() {
    std::string out;
    if (!PyErr_Occurred()) return out;
    PyObject *ptype=nullptr, *pvalue=nullptr, *ptraceback=nullptr;
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);
    PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);
    // Try traceback.format_exception
    PyObject* tb_mod = PyImport_ImportModule("traceback");
    if (tb_mod) {
        PyObject* fmt = PyObject_GetAttrString(tb_mod, "format_exception");
        if (fmt && PyCallable_Check(fmt)) {
            PyObject* lst = PyObject_CallFunctionObjArgs(fmt, ptype ? ptype : Py_None, pvalue ? pvalue : Py_None, ptraceback ? ptraceback : Py_None, nullptr);
            if (lst && PyList_Check(lst)) {
                std::ostringstream oss;
                Py_ssize_t n = PyList_Size(lst);
                for (Py_ssize_t i = 0; i < n; ++i) {
                    PyObject* li = PyList_GetItem(lst, i); // borrowed
                    const char* ci = PyUnicode_AsUTF8(li);
                    if (ci) oss << ci;
                }
                out = oss.str();
            }
            Py_XDECREF(lst);
        }
        Py_XDECREF(fmt);
        Py_DECREF(tb_mod);
    }
    if (out.empty()) {
        // Fallback: type + value
        std::string tname;
        if (ptype) {
            PyObject* name = PyObject_GetAttrString(ptype, "__name__");
            if (name) {
                const char* cn = PyUnicode_AsUTF8(name);
                if (cn) tname = cn;
                Py_DECREF(name);
            } else {
                tname = pyobj_str_utf8(ptype);
            }
        }
        std::string v = pyobj_str_utf8(pvalue);
        out = tname.empty() ? v : (tname + ": " + v);
    }
    Py_XDECREF(ptype);
    Py_XDECREF(pvalue);
    Py_XDECREF(ptraceback);
    // Do not call PyErr_Clear here; Fetch already clears the error indicator
    return out;
}

static std::string infer_prefix_from_taz(const std::string& role, const std::string& taz_table) {
    std::string s = taz_table;
    if (s.empty()) return std::string();
    std::string prefix_part = role + ".";
    std::string suffix = "other_taz_socioeconomic";
    if (s.rfind(prefix_part, 0) != 0) return std::string();
    std::string tail = s.substr(prefix_part.size());
    if (tail.size() < suffix.size()) return std::string();
    if (tail.compare(tail.size() - suffix.size(), suffix.size(), suffix) != 0) return std::string();
    std::string mid = tail.substr(0, tail.size() - suffix.size());
    if (!mid.empty() && mid.back() == '_') mid.pop_back();
    return mid;
}

// Helper: JSON escape for building combined JSON
static std::string json_escape(const std::string& in) {
    std::ostringstream oss;
    for (char c : in) {
        if (c == '"') oss << "\\\"";
        else if (c == '\\') oss << "\\\\";
        else if (c == '\n') oss << "\\n";
        else if (c == '\r') oss << "\\r";
        else if (c == '\t') oss << "\\t";
        else oss << c;
    }
    return oss.str();
}

// Helper: extract nested fields from a Python dict
static long extract_long_field(PyObject* root, const std::vector<const char*>& path, long defv = -1) {
    PyObject* cur = root;
    for (size_t i = 0; i < path.size(); ++i) {
        if (!cur || !PyDict_Check(cur)) return defv;
        PyObject* key = PyUnicode_FromString(path[i]);
        if (!key) return defv;
        PyObject* nxt = PyDict_GetItem(cur, key); // borrowed
        Py_DECREF(key);
        if (!nxt) return defv;
        cur = nxt;
    }
    if (PyLong_Check(cur)) return (long)PyLong_AsLong(cur);
    return defv;
}

static std::string extract_str_field(PyObject* root, const std::vector<const char*>& path, const std::string& defv = std::string()) {
    PyObject* cur = root;
    for (size_t i = 0; i < path.size(); ++i) {
        if (!cur || !PyDict_Check(cur)) return defv;
        PyObject* key = PyUnicode_FromString(path[i]);
        if (!key) return defv;
        PyObject* nxt = PyDict_GetItem(cur, key); // borrowed
        Py_DECREF(key);
        if (!nxt) return defv;
        cur = nxt;
    }
    const char* s = PyUnicode_Check(cur) ? PyUnicode_AsUTF8(cur) : nullptr;
    return s ? std::string(s) : defv;
}

static void print_runtime_debug(const std::string& msg) {
    // 1) stderr (if client has a console)
    try {
        std::fprintf(stderr, "%s\n", msg.c_str());
        std::fflush(stderr);
    } catch (...) {
    }

    // 2) Windows debugger output (works for GUI apps too)
    OutputDebugStringA(msg.c_str());
    OutputDebugStringA("\n");
}

static std::string mask_secret(const std::string& s) {
    if (s.empty()) return "";
    if (s.size() <= 2) return "**";
    return std::string(1, s.front()) + "***" + std::string(1, s.back());
}

static void print_cfg_db_conn(PyObject* cfg, const char* tag) {
    if (!cfg || !PyDict_Check(cfg)) return;
    std::string host = extract_str_field(cfg, {"db_conn_str", "host"}, "");
    std::string dbname = extract_str_field(cfg, {"db_conn_str", "dbname"}, "");
    std::string user = extract_str_field(cfg, {"db_conn_str", "user"}, "");
    std::string role = extract_str_field(cfg, {"db_conn_str", "role"}, "");
    std::string scenario_prefix = extract_str_field(cfg, {"db_conn_str", "scenario_prefix"}, "");
    std::string password = extract_str_field(cfg, {"db_conn_str", "password"}, "");
    long port = extract_long_field(cfg, {"db_conn_str", "port"}, -1);

    std::ostringstream oss;
    oss << "[cfg.db_conn_str]";
    if (tag && *tag) oss << " tag=" << tag;
    oss << " host=" << host
        << " port=" << port
        << " dbname=" << dbname
        << " user=" << user
        << " password=" << mask_secret(password)
        << " role=" << role
        << " scenario_prefix=" << scenario_prefix;
    print_runtime_debug(oss.str());
}

static std::wstring utf8_to_wide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring ws(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
    if (!ws.empty() && ws.back() == L'\0') ws.pop_back();
    return ws;
}

static std::string wide_to_utf8(const std::wstring& ws) {
    if (ws.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &s[0], len, nullptr, nullptr);
    if (!s.empty() && s.back() == '\0') s.pop_back();
    return s;
}

static std::wstring dirname_ws(const std::wstring& path) {
    size_t pos = path.find_last_of(L"/\\");
    if (pos == std::wstring::npos) return L".";
    return path.substr(0, pos);
}

static bool dir_exists_ws(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    if (a == INVALID_FILE_ATTRIBUTES) return false;
    return (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static bool file_exists_ws(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    if (a == INVALID_FILE_ATTRIBUTES) return false;
    return (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

#ifndef LOAD_LIBRARY_SEARCH_DEFAULT_DIRS
#define LOAD_LIBRARY_SEARCH_DEFAULT_DIRS 0x00001000
#endif
#ifndef LOAD_LIBRARY_SEARCH_USER_DIRS
#define LOAD_LIBRARY_SEARCH_USER_DIRS    0x00000400
#endif

static void configure_dll_search_paths(const std::wstring& pyhome, const std::wstring& dllDir) {
    // Dynamically resolve APIs to support older systems gracefully
    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel) {
        log_debug("[dll_search] kernel32.dll not found");
        return;
    }
    typedef PVOID (WINAPI *PFN_AddDllDirectory)(PCWSTR);
    typedef BOOL (WINAPI *PFN_SetDllDirectoryW)(LPCWSTR);
    typedef BOOL (WINAPI *PFN_RemoveDllDirectory)(PVOID);

    PFN_AddDllDirectory   pAddDllDirectory   = (PFN_AddDllDirectory)  GetProcAddress(hKernel, "AddDllDirectory");
    PFN_SetDllDirectoryW  pSetDllDirectoryW  = (PFN_SetDllDirectoryW) GetProcAddress(hKernel, "SetDllDirectoryW");
    g_RemoveDllDirectory  = (PFN_RemoveDllDirectory)                   GetProcAddress(hKernel, "RemoveDllDirectory");

    if (pAddDllDirectory) {
        g_cookie_pyhome = pAddDllDirectory(pyhome.c_str());
        log_debug(std::string("[dll_search] AddDllDirectory(pyhome) ") + (g_cookie_pyhome ? "OK" : "FAIL"));
        std::wstring pyhome_dlls = pyhome + L"\\DLLs";
        g_cookie_pyhome_dlls = pAddDllDirectory(pyhome_dlls.c_str());
        log_debug(std::string("[dll_search] AddDllDirectory(pyhome\\DLLs) ") + (g_cookie_pyhome_dlls ? "OK" : "FAIL"));
        g_cookie_dlldir = pAddDllDirectory(dllDir.c_str());
        log_debug(std::string("[dll_search] AddDllDirectory(dist_dll) ") + (g_cookie_dlldir ? "OK" : "FAIL"));
    } else if (pSetDllDirectoryW) {
        // Fallback: this replaces the default search path with the directory
        BOOL ok = pSetDllDirectoryW(pyhome.c_str());
        log_debug(std::string("[dll_search] SetDllDirectoryW(pyhome) ") + (ok ? "OK" : "FAIL"));
    } else {
        log_debug("[dll_search] No AddDllDirectory/SetDllDirectoryW available");
    }
}

static void reset_dll_search_paths() {
    if (g_RemoveDllDirectory) {
        if (g_cookie_pyhome) { g_RemoveDllDirectory(g_cookie_pyhome); g_cookie_pyhome=nullptr; log_debug("[dll_search] RemoveDllDirectory(pyhome)"); }
        if (g_cookie_pyhome_dlls) { g_RemoveDllDirectory(g_cookie_pyhome_dlls); g_cookie_pyhome_dlls=nullptr; log_debug("[dll_search] RemoveDllDirectory(pyhome\\DLLs)"); }
        if (g_cookie_dlldir) { g_RemoveDllDirectory(g_cookie_dlldir); g_cookie_dlldir=nullptr; log_debug("[dll_search] RemoveDllDirectory(dist_dll)"); }
    }
}

static void preload_runtime_dlls(const std::wstring& pyhome) {
    // Minimal preload: only Python DLLs to avoid global runtime conflicts
    const wchar_t* names[] = {
        L"python311.dll",
        L"python3.dll"
    };
    for (auto name : names) {
        std::wstring path = pyhome + L"\\" + name;
        HMODULE h = LoadLibraryW(path.c_str());
        std::string nn = wide_to_utf8(name);
        log_debug(std::string("[preload] ") + nn + ": " + (h ? "OK" : "FAIL"));
    }
}

static void set_python_home_from_dll() {
    // If an embedded runtime is placed at <project_root>\dist_dll\pyhome,
    // point Python to that directory so the client is portable.
    wchar_t dll_path[MAX_PATH];
    HMODULE hm = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)&set_python_home_from_dll, &hm)) {
        return;
    }
    if (!GetModuleFileNameW(hm, dll_path, MAX_PATH)) {
        return;
    }
    std::wstring dllDir = dirname_ws(dll_path);           // ...\dist_dll

    // FIX: Py_SetPythonHome 要求指针在整个进程生命周期内有效
    // 使用 static 变量避免悬空指针导致闪退
    static std::wstring s_pyhome;
    s_pyhome = dllDir + L"\\pyhome";  // ...\dist_dll\pyhome

    // cache for PyConfig usage
    g_pyhome_ws = s_pyhome;
    g_dlldir_ws = dllDir;

    log_debug("[set_python_home] DLL Dir: " + wide_to_utf8(dllDir));
    log_debug("[set_python_home] PyHome: " + wide_to_utf8(s_pyhome));

    // Diagnostic checks before initializing Python
    std::wstring p_zip = s_pyhome + L"\\python311.zip";
    std::wstring p_lib_os = s_pyhome + L"\\Lib\\os.py";
    std::wstring p_lib_site = s_pyhome + L"\\Lib\\site.py";
    std::wstring p_enc = s_pyhome + L"\\Lib\\encodings\\__init__.py";
    std::wstring p_sitepkgs = s_pyhome + L"\\Lib\\site-packages";
    log_debug(std::string("[check] python311.zip: ") + (file_exists_ws(p_zip) ? "OK" : "MISSING"));
    log_debug(std::string("[check] Lib/os.py: ") + (file_exists_ws(p_lib_os) ? "OK" : "MISSING"));
    log_debug(std::string("[check] Lib/site.py: ") + (file_exists_ws(p_lib_site) ? "OK" : "MISSING"));
    log_debug(std::string("[check] Lib/encodings/__init__.py: ") + (file_exists_ws(p_enc) ? "OK" : "MISSING"));
    log_debug(std::string("[check] Lib/site-packages dir: ") + (dir_exists_ws(p_sitepkgs) ? "OK" : "MISSING"));

    if (dir_exists_ws(s_pyhome)) {
        log_debug("[set_python_home] PyHome exists, setting...");
        Py_SetPythonHome(const_cast<wchar_t*>(s_pyhome.c_str()));

        // Configure DLL search paths so we don't need to copy runtime DLLs to the exe dir
        configure_dll_search_paths(s_pyhome, dllDir);

        // Preload minimal python DLLs
        preload_runtime_dlls(s_pyhome);

        // Proactively load python DLLs from pyhome to avoid picking up wrong ones from PATH
        std::wstring py_dll = s_pyhome + L"\\python311.dll";
        HMODULE hPy = LoadLibraryW(py_dll.c_str());
        log_debug(std::string("[dll_search] LoadLibraryW(python311.dll) ") + (hPy ? "OK" : "FAIL"));
        std::wstring py3_dll = s_pyhome + L"\\python3.dll";
        HMODULE hPy3 = LoadLibraryW(py3_dll.c_str());
        log_debug(std::string("[dll_search] LoadLibraryW(python3.dll) ") + (hPy3 ? "OK" : "FAIL"));
    } else {
        log_debug("[set_python_home] WARNING: PyHome NOT found!");
    }
}

static void ensure_python_initialized() {
    if (!Py_IsInitialized()) {
        log_debug("[ensure_python_initialized] Python not initialized, starting...");
        // Log which python DLLs are currently loaded (to catch mismatches)
        std::wstring loaded_py_dll = get_loaded_module_path(L"python311.dll");
        std::wstring loaded_py3_dll = get_loaded_module_path(L"python3.dll");
        log_debug(std::string("[ensure_python_initialized] loaded python311.dll: ") + (loaded_py_dll.empty() ? "<not loaded>" : wide_to_utf8(loaded_py_dll)));
        log_debug(std::string("[ensure_python_initialized] loaded python3.dll: ") + (loaded_py3_dll.empty() ? "<not loaded>" : wide_to_utf8(loaded_py3_dll)));

        set_python_home_from_dll();

        // Use new config API to explicitly set module search paths to avoid init failures
        log_debug("[ensure_python_initialized] Using PyConfig to initialize...");
        PyStatus status;
        PyConfig config;
        PyConfig_InitIsolatedConfig(&config); // no env vars influence
        config.parse_argv = 0;
        config.install_signal_handlers = 0;
        config.site_import = 1; // allow site
        config.user_site_directory = 0;
        config.use_environment = 0;

        if (!g_pyhome_ws.empty()) {
            status = PyConfig_SetString(&config, &config.home, g_pyhome_ws.c_str());
            if (PyStatus_Exception(status)) {
                log_debug("[config] Set home FAILED");
            } else {
                log_debug("[config] home=" + wide_to_utf8(g_pyhome_ws));
            }
        }

        config.module_search_paths_set = 1;
        auto append_path = [&](const std::wstring& p){
            if (p.empty()) return;
            PyStatus st = PyWideStringList_Append(&config.module_search_paths, p.c_str());
            std::string ps = wide_to_utf8(p);
            log_debug(std::string("[config] append path: ") + ps + (PyStatus_Exception(st) ? " (FAIL)" : " (OK)"));
        };
        if (!g_pyhome_ws.empty()) {
            append_path(g_pyhome_ws + L"\\python311.zip");
            append_path(g_pyhome_ws + L"\\DLLs");
            append_path(g_pyhome_ws + L"\\Lib");
            append_path(g_pyhome_ws + L"\\Lib\\site-packages");
        }
        if (!g_dlldir_ws.empty()) append_path(g_dlldir_ws);

        status = Py_InitializeFromConfig(&config);
        if (PyStatus_Exception(status)) {
            log_debug("[ensure_python_initialized] Py_InitializeFromConfig FAILED!");
        }
        PyConfig_Clear(&config);

        if (Py_IsInitialized()) {
            log_debug("[ensure_python_initialized] Py_Initialize SUCCESS");

            // IMPORTANT:
            // Py_InitializeFromConfig leaves the GIL held by the initializing thread.
            // If we don't release it, a later call from a different OS thread can hang
            // forever at PyGILState_Ensure().
            if (!g_gil_released_after_init) {
                PyEval_SaveThread();
                g_gil_released_after_init = true;
                log_debug("[ensure_python_initialized] GIL released after init (PyEval_SaveThread)");
            }
        } else {
            log_debug("[ensure_python_initialized] Py_Initialize FAILED!");
        }
    } else {
        log_debug("[ensure_python_initialized] Python already initialized");
    }
}

static void add_sys_path(const std::wstring& p) {
    log_debug("[add_sys_path] Adding: " + wide_to_utf8(p));
    PyObject* sys_path = PySys_GetObject((char*)"path"); // borrowed
    if (!sys_path) {
        log_debug("[add_sys_path] ERROR: sys.path is NULL!");
        return;
    }
    PyObject* ws = PyUnicode_FromWideChar(p.c_str(), (Py_ssize_t)p.size());
    if (!ws) {
        log_debug("[add_sys_path] ERROR: PyUnicode_FromWideChar failed!");
        return;
    }
    PyList_Insert(sys_path, 0, ws);
    Py_DECREF(ws);
    log_debug("[add_sys_path] OK");
}

static void setup_sys_path_relative_to_this_dll() {
    wchar_t dll_path[MAX_PATH];
    HMODULE hm = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)&setup_sys_path_relative_to_this_dll, &hm)) {
        return;
    }
    if (!GetModuleFileNameW(hm, dll_path, MAX_PATH)) {
        return;
    }
    std::wstring dllDir = dirname_ws(dll_path);
    // 期望 DLL 位于 <project_root>\\dist_dll\\trip_model.dll
    std::wstring parent = dirname_ws(dllDir); // 上一级作为 project_root
    // add_sys_path 会插入到 sys.path[0]，所以调用顺序需要反过来：
    // 先插入 parent，再插入 dllDir，确保 dllDir 的包优先级更高。
    add_sys_path(parent);
    add_sys_path(dllDir);
}

static void refresh_python_import_state() {
    if (!Py_IsInitialized()) return;
    const char* code =
        "import sys, importlib, os, shutil\n"
        "sys.dont_write_bytecode = True\n"
        "try:\n"
        "    importlib.invalidate_caches()\n"
        "except Exception:\n"
        "    pass\n"
        "# purge modules to force reload\n"
        "for name in list(sys.modules.keys()):\n"
        "    if name == 'trip_generation' or name.startswith('trip_generation.') or name == 'trip_distribution' or name.startswith('trip_distribution.'):\n"
        "        sys.modules.pop(name, None)\n"
        "# best-effort remove __pycache__ for these packages on sys.path\n"
        "for base in list(sys.path):\n"
        "    for pkg in ('trip_generation', 'trip_distribution'):\n"
        "        p = os.path.join(base, pkg, '__pycache__')\n"
        "        if os.path.isdir(p):\n"
        "            try:\n"
        "                shutil.rmtree(p, ignore_errors=True)\n"
        "            except Exception:\n"
        "                pass\n";
    PyRun_SimpleString(code);
}

static PyObject* json_loads(const std::string& cfg_json) {
    PyObject* json_mod = PyImport_ImportModule("json");
    if (!json_mod) return nullptr;
    PyObject* loads = PyObject_GetAttrString(json_mod, "loads");
    Py_DECREF(json_mod);
    if (!loads) return nullptr;
    PyObject* arg = PyUnicode_FromString(cfg_json.c_str());
    if (!arg) { Py_DECREF(loads); return nullptr; }
    PyObject* cfg = PyObject_CallFunctionObjArgs(loads, arg, nullptr);
    Py_DECREF(arg);
    Py_DECREF(loads);
    return cfg; // may be nullptr
}

static int call_py_generation(const char* cfg_json,
                              const char* taz, char** out_json) {
    std::lock_guard<std::mutex> _guard(g_call_mutex);
    g_progress_cb = nullptr;
    g_scale_a = 1.0; g_scale_b = 0.0;
    ensure_python_initialized();
    PyGILState_STATE gstate = PyGILState_Ensure();
    setup_sys_path_relative_to_this_dll();
    refresh_python_import_state();

    int rc = 0;
    do {
        PyObject* mod = PyImport_ImportModule("trip_generation.trip_generation");
        if (!mod) { rc = 1; break; }
        PyObject* fn = PyObject_GetAttrString(mod, "fs_run_trip_generation");
        Py_DECREF(mod);
        if (!fn || !PyCallable_Check(fn)) { rc = 2; Py_XDECREF(fn); break; }

        // 生成阶段进度缩放到 0..50
        PyObject* cb = create_progress_callable_gen();
        if (!cb) { rc = 3; Py_DECREF(fn); break; }

        PyObject* cfg = json_loads(cfg_json ? cfg_json : "{}");
        if (!cfg) { rc = 4; Py_DECREF(fn); Py_DECREF(cb); break; }

        // Print db conn config received from caller (not file log)
        print_cfg_db_conn(cfg, "run_trip_generation");

        PyObject* taz_str = PyUnicode_FromString(taz ? taz : "");
        if (!taz_str) { rc = 5; Py_DECREF(fn); Py_DECREF(cb); Py_DECREF(cfg); break; }

        PyObject* ret = PyObject_CallFunctionObjArgs(fn, cb, cfg, taz_str, nullptr);
        Py_DECREF(fn);
        Py_DECREF(cb);
        Py_DECREF(cfg);
        Py_DECREF(taz_str);

        if (!ret) { rc = 6; break; }
        if (!PyUnicode_Check(ret)) { Py_DECREF(ret); rc = 7; break; }

        std::string json_out = PyUnicode_AsUTF8(ret);
        Py_DECREF(ret);
        set_out_json(out_json, json_out);
    } while (0);

    if (rc != 0) {
        std::string detail = fetch_python_error_string();
        if (out_json) {
            std::string msg = "trip_generation failed, rc=" + std::to_string(rc);
            if (!detail.empty()) msg += ", pyerr: " + detail;
            set_out_json(out_json, json_error(msg));
        }
    }

    g_scale_a = 1.0; g_scale_b = 0.0; g_iter_base = 0; g_last_iter = 0;
    PyGILState_Release(gstate);
    reset_dll_search_paths();
    return rc;
}

static int call_py_distribution(const char* cfg_json,
                                const char* taz, const char* pa, char** out_json) {
    std::lock_guard<std::mutex> _guard(g_call_mutex);
    g_progress_cb = nullptr;
    g_scale_a = 1.0; g_scale_b = 0.0;
    ensure_python_initialized();
    PyGILState_STATE gstate = PyGILState_Ensure();
    setup_sys_path_relative_to_this_dll();
    refresh_python_import_state();

    int rc = 0;
    do {
        PyObject* mod = PyImport_ImportModule("trip_distribution.trip_distribution");
        if (!mod) { rc = 1; break; }
        PyObject* fn = PyObject_GetAttrString(mod, "fs_run_trip_distribution");
        Py_DECREF(mod);
        if (!fn || !PyCallable_Check(fn)) { rc = 2; Py_XDECREF(fn); break; }

        PyObject* cb = create_progress_callable();
        if (!cb) { rc = 3; Py_DECREF(fn); break; }

        PyObject* cfg = json_loads(cfg_json ? cfg_json : "{}");
        if (!cfg) { rc = 4; Py_DECREF(fn); Py_DECREF(cb); break; }

        // Print db conn config received from caller (not file log)
        print_cfg_db_conn(cfg, "run_trip_distribution");

        PyObject* taz_str = PyUnicode_FromString(taz ? taz : "");
        PyObject* pa_str = PyUnicode_FromString(pa ? pa : "");
        if (!taz_str || !pa_str) { rc = 5; Py_DECREF(fn); Py_DECREF(cb); Py_DECREF(cfg); Py_XDECREF(taz_str); Py_XDECREF(pa_str); break; }

        PyObject* ret = PyObject_CallFunctionObjArgs(fn, cb, cfg, taz_str, pa_str, nullptr);
        Py_DECREF(fn);
        Py_DECREF(cb);
        Py_DECREF(cfg);
        Py_DECREF(taz_str);
        Py_DECREF(pa_str);

        if (!ret) { rc = 6; break; }
        if (!PyUnicode_Check(ret)) { Py_DECREF(ret); rc = 7; break; }

        std::string json_out = PyUnicode_AsUTF8(ret);
        Py_DECREF(ret);
        set_out_json(out_json, json_out);
    } while (0);

    if (rc != 0) {
        std::string detail = fetch_python_error_string();
        if (out_json) {
            std::string msg = "trip_distribution failed, rc=" + std::to_string(rc);
            if (!detail.empty()) msg += ", pyerr: " + detail;
            set_out_json(out_json, json_error(msg));
        }
    }

    PyGILState_Release(gstate);
    reset_dll_search_paths();
    return rc;
}

// Run both generation then distribution in a single call
static int call_py_model_all(void (*on_progress)(int,double), const char* cfg_json,
                             const char* taz, const char* hist_od, /*out*/ char** out_json) {
    std::lock_guard<std::mutex> _guard(g_call_mutex);
    log_debug("[call_py_model_all] === ENTRY ===");
    g_progress_cb = on_progress;
    g_scale_a = 1.0; g_scale_b = 0.0;
    g_iter_base = 0; g_last_iter = 0;

    log_debug("[call_py_model_all] Calling ensure_python_initialized...");
    ensure_python_initialized();

    log_debug("[call_py_model_all] Acquiring GIL...");
    PyGILState_STATE gstate = PyGILState_Ensure();
    log_debug("[call_py_model_all] GIL acquired");

    log_debug("[call_py_model_all] Setting up sys.path...");
    setup_sys_path_relative_to_this_dll();
    log_debug("[call_py_model_all] sys.path setup done");
    refresh_python_import_state();

    int rc = 0;
    std::string final_json;
    do {
        PyObject* cfg = json_loads(cfg_json ? cfg_json : "{}");
        if (!cfg) { rc = 10; break; }

        // Print db conn config received from caller (not file log)
        print_cfg_db_conn(cfg, "run_trip_model_all");
        PyObject* taz_str = PyUnicode_FromString(taz ? taz : "");
        if (!taz_str) { Py_DECREF(cfg); rc = 11; break; }
        PyObject* hist_od_str = PyUnicode_FromString(hist_od ? hist_od : "");
        if (!hist_od_str) { Py_DECREF(cfg); Py_DECREF(taz_str); rc = 11; break; }

        // 1) generation
        PyObject* mod_gen = PyImport_ImportModule("trip_generation.trip_generation");
        if (!mod_gen) { Py_DECREF(cfg); Py_DECREF(taz_str); rc = 12; break; }
        PyObject* fn_gen = PyObject_GetAttrString(mod_gen, "fs_run_trip_generation");
        Py_DECREF(mod_gen);
        if (!fn_gen || !PyCallable_Check(fn_gen)) { Py_XDECREF(fn_gen); Py_DECREF(cfg); Py_DECREF(taz_str); rc = 13; break; }
    // generation callbacks mapped to 0..50
        // 迭代号显示为 1..50，方便 UI 统一进度条
        PyObject* cb = create_progress_callable_gen();
        if (!cb) { Py_DECREF(fn_gen); Py_DECREF(cfg); Py_DECREF(taz_str); rc = 14; break; }
        PyObject* ret_gen = PyObject_CallFunctionObjArgs(fn_gen, cb, cfg, taz_str, nullptr);
        Py_DECREF(fn_gen);
        Py_DECREF(cb);
        if (!ret_gen) { Py_DECREF(cfg); Py_DECREF(taz_str); rc = 15; break; }
        if (!PyUnicode_Check(ret_gen)) { Py_DECREF(ret_gen); Py_DECREF(cfg); Py_DECREF(taz_str); rc = 16; break; }
        std::string gen_json = PyUnicode_AsUTF8(ret_gen);
        log_debug_large("[gen_json]", gen_json);

        PyObject* gen_obj = json_loads(gen_json);
        long gen_status = 0;
        std::string gen_message;
        std::string pa_table;
        long pa_rows = -1;
        if (gen_obj) {
            gen_status = extract_long_field(gen_obj, {"status"}, 0);
            gen_message = extract_str_field(gen_obj, {"message"}, "");
            pa_table = extract_str_field(gen_obj, {"data","tables","trip_generation_table"}, "");
            if (pa_table.empty()) pa_table = extract_str_field(gen_obj, {"tables","trip_generation_table"}, "");
            pa_rows = extract_long_field(gen_obj, {"data","counts","pa_rows"}, -1);
        }

        // 2) distribution
        std::string dist_json;
        long dist_status = 0;
        std::string dist_message;
        std::string od_table;
        long od_rows = -1;
        long hist_used_summary = -1;
        long hist_rows_summary = -1;
        if (gen_status == 1 && !pa_table.empty()) {
            PyObject* mod_dist = PyImport_ImportModule("trip_distribution.trip_distribution");
            if (!mod_dist) { Py_XDECREF(gen_obj); Py_DECREF(ret_gen); Py_DECREF(cfg); Py_DECREF(taz_str); rc = 17; break; }
            PyObject* fn_dist = PyObject_GetAttrString(mod_dist, "fs_run_trip_distribution");
            Py_DECREF(mod_dist);
            if (!fn_dist || !PyCallable_Check(fn_dist)) { Py_XDECREF(fn_dist); Py_XDECREF(gen_obj); Py_DECREF(ret_gen); Py_DECREF(cfg); Py_DECREF(taz_str); rc = 18; break; }
            // 分布阶段进度缩放到 50..100
            g_iter_base = g_last_iter; // 让分布阶段迭代号在生成阶段后继续递增
            PyObject* cb2 = create_progress_callable_dist();
            if (!cb2) { Py_DECREF(fn_dist); Py_XDECREF(gen_obj); Py_DECREF(ret_gen); Py_DECREF(cfg); Py_DECREF(taz_str); rc = 19; break; }
            PyObject* pa_str = PyUnicode_FromString(pa_table.c_str());
            if (!pa_str) { Py_DECREF(fn_dist); Py_DECREF(cb2); Py_XDECREF(gen_obj); Py_DECREF(ret_gen); Py_DECREF(cfg); Py_DECREF(taz_str); Py_DECREF(hist_od_str); rc = 20; break; }
            PyObject* ret_dist = PyObject_CallFunctionObjArgs(fn_dist, cb2, cfg, taz_str, pa_str, hist_od_str, nullptr);
            if (!ret_dist) {
                PyErr_Clear();
                ret_dist = PyObject_CallFunctionObjArgs(fn_dist, cb2, cfg, taz_str, pa_str, nullptr);
            }
            Py_DECREF(fn_dist);
            Py_DECREF(cb2);
            Py_DECREF(pa_str);
            if (!ret_dist) { Py_XDECREF(gen_obj); Py_DECREF(ret_gen); Py_DECREF(cfg); Py_DECREF(taz_str); rc = 21; break; }
            if (!PyUnicode_Check(ret_dist)) { Py_DECREF(ret_dist); Py_XDECREF(gen_obj); Py_DECREF(ret_gen); Py_DECREF(cfg); Py_DECREF(taz_str); rc = 22; break; }
            dist_json = PyUnicode_AsUTF8(ret_dist);
            log_debug_large("[dist_json]", dist_json);

            PyObject* dist_obj = json_loads(dist_json);
            if (dist_obj) {
                dist_status = extract_long_field(dist_obj, {"status"}, 0);
                dist_message = extract_str_field(dist_obj, {"message"}, "");
                od_table = extract_str_field(dist_obj, {"data","tables","trip_distribution_table"}, "");
                if (od_table.empty()) od_table = extract_str_field(dist_obj, {"tables","trip_distribution_table"}, "");
                od_rows = extract_long_field(dist_obj, {"data","counts","od_rows"}, -1);
                // read hist usage summary if provided by python side
                hist_used_summary = extract_long_field(dist_obj, {"summary","hist_od_used"}, -1);
                hist_rows_summary = extract_long_field(dist_obj, {"summary","hist_od_rows"}, -1);
            }
            Py_XDECREF(dist_obj);
            Py_DECREF(ret_dist);
        }

        long final_status = (gen_status == 1 && dist_status == 1) ? 1 : 0;
        std::string final_message = (final_status == 1) ? "Success" : (gen_status != 1 ? (std::string("Trip Generation failed: ") + gen_message) : (std::string("Trip Distribution failed: ") + dist_message));

        // Prepare input echo
        std::string taz_in = taz ? std::string(taz) : std::string("");
        const char* hist_in_c = PyUnicode_AsUTF8(hist_od_str);
        std::string hist_in = hist_in_c ? std::string(hist_in_c) : std::string("");
        std::string role_in = extract_str_field(cfg, {"db_conn_str","role"}, "");
        std::string prefix_in_cfg = extract_str_field(cfg, {"db_conn_str","scenario_prefix"}, "");
        std::string prefix_in = infer_prefix_from_taz(role_in, taz_in);
        if (prefix_in.empty()) prefix_in = prefix_in_cfg;
        std::string host_in = extract_str_field(cfg, {"db_conn_str","host"}, "");
        long port_in = extract_long_field(cfg, {"db_conn_str","port"}, 0);
        std::string dbname_in = extract_str_field(cfg, {"db_conn_str","dbname"}, "");
        std::string user_in = extract_str_field(cfg, {"db_conn_str","user"}, "");
        // 明确打印实际使用的表名到日志
        log_debug("[result] Table Summary:");
        log_debug("[result]   Input TAZ: " + json_escape(taz_in));
        log_debug("[result]   Output Gen (PA): " + json_escape(pa_table));
        log_debug("[result]   Output Dist (OD): " + json_escape(od_table));
        log_debug("[result]   DB: " + role_in + "." + prefix_in + "* (cfg=" + prefix_in_cfg + ")");
        std::string sep_hist = "";
        if (!prefix_in.empty() && prefix_in.back() != '_') sep_hist = "_";
        std::string default_hist = role_in + std::string(".") + prefix_in + sep_hist + std::string("other_trip_distribution_hist");
        // algorithm params echo
        double param_lambda_in = extract_double_field(cfg, {"algorithm_params","param_lambda"}, 0.1);
        double diag_eps_in = extract_double_field(cfg, {"algorithm_params","diag_eps"}, 0.1);
        long   max_iter_in = extract_long_field(cfg, {"algorithm_params","max_iter"}, 20);
        double tol_in = extract_double_field(cfg, {"algorithm_params","tol"}, 1e-6);
        double lambda_min_in = extract_double_field(cfg, {"algorithm_params","lambda_min"}, 0.01);
        double lambda_max_in = extract_double_field(cfg, {"algorithm_params","lambda_max"}, 1.0);
        long   lambda_steps_in = extract_long_field(cfg, {"algorithm_params","lambda_steps"}, 10);

        std::ostringstream oss;
        oss << "{\"status\":" << final_status
            << ",\"message\":\"" << json_escape(final_message) << "\",";
        oss << "\"data\":{\"tables\":{\"trip_generation_table\":\"" << json_escape(pa_table) << "\","
            << "\"trip_distribution_table\":\"" << json_escape(od_table) << "\"},"
            << "\"counts\":{\"pa_rows\":" << pa_rows << ",\"od_rows\":" << od_rows << "}},";
        oss << "\"summary\":{\"gen_status\":" << gen_status
            << ",\"dist_status\":" << dist_status
            << ",\"gen_message\":\"" << json_escape(gen_message) << "\""
            << ",\"dist_message\":\"" << json_escape(dist_message) << "\""
            << ",\"tables\":{\"trip_generation_table\":\"" << json_escape(pa_table)
            << "\",\"trip_distribution_table\":\"" << json_escape(od_table) << "\"}},";
        // inputs echo (sanitized)
        oss << "\"inputs\":{"
            << "\"taz_table\":\"" << json_escape(taz_in) << "\","
            << "\"hist_od_table_given\":\"" << json_escape(hist_in) << "\","
            << "\"default_hist_od_table\":\"" << json_escape(default_hist) << "\","
            << "\"hist_od_used\":" << hist_used_summary << ",\"hist_od_rows\":" << hist_rows_summary << ","
            << "\"db\":{\"host\":\"" << json_escape(host_in) << "\",\"port\":" << port_in
            << ",\"dbname\":\"" << json_escape(dbname_in) << "\",\"user\":\"" << json_escape(user_in) << "\",\"role\":\"" << json_escape(role_in) << "\",\"scenario_prefix\":\"" << json_escape(prefix_in) << "\",\"cfg_scenario_prefix\":\"" << json_escape(prefix_in_cfg) << "\"},"
            << "\"algorithm_params\":{\"param_lambda\":" << param_lambda_in
            << ",\"diag_eps\":" << diag_eps_in
            << ",\"max_iter\":" << max_iter_in
            << ",\"tol\":" << tol_in
            << ",\"lambda_min\":" << lambda_min_in
            << ",\"lambda_max\":" << lambda_max_in
            << ",\"lambda_steps\":" << lambda_steps_in << "}}";
        oss << "}";
        final_json = oss.str();
        log_debug_large("[final_json]", final_json);

        Py_XDECREF(gen_obj);
        Py_DECREF(ret_gen);
        Py_DECREF(cfg);
        Py_DECREF(taz_str);
        Py_DECREF(hist_od_str);
    } while (0);

    if (rc != 0) {
        std::string detail = fetch_python_error_string();
        std::string msg = "trip_model_all failed, rc=" + std::to_string(rc);
        if (!detail.empty()) msg += ", pyerr: " + detail;
        set_out_json(out_json, json_error(msg));
    } else {
        set_out_json(out_json, final_json);
    }

    PyGILState_Release(gstate);
    reset_dll_search_paths();
    return rc;
}

extern "C" __declspec(dllexport)
int run_trip_generation(const char* cfg_json, const char* taz, /*out*/ char** out_json) {
    return call_py_generation(cfg_json, taz, out_json);
}

extern "C" __declspec(dllexport)
int run_trip_distribution(const char* cfg_json, const char* taz, const char* pa, /*out*/ char** out_json) {
    return call_py_distribution(cfg_json, taz, pa, out_json);
}

extern "C" __declspec(dllexport)
void free_json(char* p) {
    if (p) std::free(p);
}

extern "C" __declspec(dllexport)
int run_trip_model_all(void (*on_progress)(int,double), const char* cfg_json, const char* taz, const char* hist_od, /*out*/ char** out_json) {
    return call_py_model_all(on_progress, cfg_json, taz, hist_od, out_json);
}
