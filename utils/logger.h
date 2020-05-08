#pragma once

#ifdef WIN32
#include <winsock2.h>
#ifndef _WINDOWS_
//#define NOMINMAX
#include <windows.h>
#endif
#else
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define closesocket(x) close(x)
#endif

enum LogLevel {T_S = 0 /**< Informazioni di supervisione che devono passare sempre */, 
               T_E /**< Errore grave */, 
               T_W /**< Informazione importante */,
               T_I /**< Informazione */, 
               T_D /**< Informazione utile ai fini di debug */
              };

#ifdef __cplusplus
extern "C" {
#endif
    void debugf(const char *, int, int, const char *, ...);
#ifdef __cplusplus
}
#endif

#define slog(args,...) debugf(__FILE__, __LINE__, T_S, ##args)
#define elog(args,...) debugf(__FILE__, __LINE__, T_E, ##args)
#define wlog(args,...) debugf(__FILE__, __LINE__, T_W, ##args)
#define ilog(args,...) debugf(__FILE__, __LINE__, T_I, ##args)
#define dlog(args,...) debugf(__FILE__, __LINE__, T_D, ##args)

#ifdef __cplusplus

#include <functional>
#include <iostream>
#include <sstream>
#include <iomanip>

#include <mutex>

// oggetto logger

#define Trace(x, args,...) Logger::Instance().trace(__FILE__, __LINE__, x, ##args)
#define VTrace(x, y, z) Logger::Instance().vtrace(__FILE__, __LINE__, x, y, z)
#define SetLog Logger::Instance().set
#define SetAppName(x) Logger::Instance().name(x)
#define LOG_INSTANCE Logger::Instance()

class Logger
{
    public:
        typedef std::function<void(const std::string &file, int line, int level, const std::string &name, const std::string &body)> LogCbk;
        //typedef void (*LogCbk)(const std::string &file, int line, int level, const std::string &name, const std::string &body);

        enum LogDest {LogToFile, LogToSystem, LogToRemote, LogToStdout, LogToStderr, LogToCustom, LogToNone};
        typedef void (*RegisterLogCbk)(const std::string &appname, int level);
    private:
        Logger() : 
            level_(T_I), size_(512 * 1024), name_("debug"), 
            app_("unknown"), ext_(".log"), dest_(LogToFile),
            socket_(-1), cbk_(NULL), register_(NULL) {}

        std::mutex cs_;
        int level_;
        int size_;
        char logbuffer_[4096];
        std::string name_;
        std::string host_;
        std::string app_;
        std::string ext_;
        LogDest dest_;
        int socket_;
        LogCbk cbk_;
        sockaddr_in address_;

        std::string remote_addr_;
        unsigned short remote_port_;
        RegisterLogCbk register_;

        void PrintDebugLog(const std::string &logname, const std::string &line);
        void FormatString(const char *file, int line, int level, const std::string &logname);

    public:
        void RemoteLog(const std::string &line);
        void RemoteLog(const void *line, size_t len);
        static Logger &Instance() {
            static Logger *instance = NULL;
            if (!instance)
                instance = new Logger();

            return *instance;
        }
#if __GNUC__ >= 3 && !defined(WIN32)
        void trace(const char *, int, int, const char *, ...) __attribute__ ((format(printf,5,6)));
#else
        void trace(const char *, int, int, const char *, ...);
#endif
        void vtrace(const char *, int, int, const char *, va_list);
        void trace(const char *, int, int, const std::string &);

        void set(const std::string &log_name, const std::string &application_name, 
                    int min_level = T_I, int max_size = 500000);

        LogDest dest() const { return dest_; }
        void dest(LogDest d) { dest_ = d; }
        int level() const { return level_; }
        void level(int l);
        void name(const std::string &n);
        const std::string &name() const { return app_; }

        void callback(LogCbk cbk) {
            dest_ = LogToCustom;
            cbk_ = cbk;
        }
        void register_log(RegisterLogCbk cbk) {
            register_ = cbk;
        }

        void remote(const std::string &host, unsigned short port);

        void ext(const std::string &e) { ext_ = e; }
        const std::string &ext() { return ext_; }
        const std::string &appname() const { return app_; }
};


class CLog 
{
    std::string m_filename;
    int m_linenum, m_level;
    bool m_enabled;
    std::ostringstream m_os;

public:
    template <typename T>
    CLog &operator<<(const T &t) {
        if (m_enabled)
            m_os << t;
        return *this;
    }
    ~CLog() {
         if (m_enabled) 
             Logger::Instance().trace(m_filename.c_str(), m_linenum, m_level, m_os.str());                 
    }

    CLog(const std::string &file, int line, int level) : m_filename(file), m_linenum(line), m_level(level) {
        m_enabled = (level <= Logger::Instance().level());
    }
};

#define ILOG       CLog(__FILE__, __LINE__, T_I)
#define DLOG       CLog(__FILE__, __LINE__, T_D)
#define WLOG       CLog(__FILE__, __LINE__, T_W)
#define ELOG       CLog(__FILE__, __LINE__, T_E)
#define SLOG       CLog(__FILE__, __LINE__, T_S)

inline std::string classMethodStr(const std::string& str_i)
{
    /* tolgo tutto cio' che segue la prima ( */
    std::string s = str_i.substr(0, str_i.find("(")); 
    std::string::size_type funcScopeOp = 0, classScopeOp = 0;
    if ((funcScopeOp = s.rfind("::", s.size())) == std::string::npos)
        return s;
    
    if ((classScopeOp = s.rfind("::", funcScopeOp - 1)) == std::string::npos)
        return s;
    
    return s.substr(classScopeOp + 2, s.size());
}

#ifndef __CLASS_METHOD__
#define __CLASS_METHOD__ classMethodStr(__PRETTY_FUNCTION__)
#endif
#endif /* cplusplus */
