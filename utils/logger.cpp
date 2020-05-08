#include "logger.h"

#include "thread.h"
#include <cstring>

#ifndef WIN32
#include <syslog.h>
#else

#pragma warning( disable : 4996 )
#undef localtime_r
#define localtime_r(x, y) localtime(x)
#define IGNORE_REMOTE_LOG

#endif


static const char *errlevel[] = { "SYS", "ERR", "WRN", "INF", "DBG", "UND", "UND" };

#include <cstdarg>

// This should be resolved better in the future

// API C per il logger
extern "C" {
    void debugf(const char *f, int line, int lvl, const char *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        Logger::Instance().vtrace(f, line, lvl, fmt, ap);
        va_end(ap);
    }
}

void Logger::
name(const std::string &name)
{
    app_ = name;

    if (register_)
        register_(name_, level_);
}

void Logger::
level(int l)
{
    level_ = l;

    if (register_)
        register_(name_, level_);
}

void Logger::
set(const std::string &name,  const std::string &appn, int minlevel, int maxsize)
{
    app_ = appn;

    if(!name.empty())
        name_ = name;

    if(minlevel > 0)
        level_ = minlevel;

    if(maxsize > 0)
        size_ = maxsize;

    if (register_)
        register_(name_, level_);
}

/*!
  \brief stampa un messaggio di log gia' formattato.

  Questa funzione scrive sul file di log la stringa passata come argomento, nel caso le
  dimensioni del file di log abbiano superato LogSize il file di log viene rinominato
  come debug0.log .. debug9.log e viene iniziata la scrittura di un nuovo file di log.
  Nel caso esistano tutti e 10 i file di log di backup viene cancellato il piu' vecchio.
  \param string stringa da scrivere sul log.
  \return void
  \remarks il file di log piu' recente e` sempre quello senza numero nel nome.
*/
void Logger::
PrintDebugLog(const std::string &logname, const std::string &dst)
{
	FILE *f;
	unsigned long l;

	if (! (f = fopen(logname.c_str(), "a")))
		return;

	fseek(f, 0, SEEK_END);		// necessaria per come lavora ftell...
	l = ftell(f);

	if ((long)l > size_) {
		fclose(f);

        //struct tm mytm;
        time_t now = time(NULL);
        // NOTE: only windows has a threadsafe localtime that does not need this
#ifndef WIN32
        struct tm mytm;
#endif
        if (struct tm *res = localtime_r(&now, &mytm)) {
            char buffer[100];

            strftime(buffer, sizeof(buffer), "_%Y%m%d%H%M%S", res);

            std::string destname = logname.substr(0, logname.length() - ext_.size()) +
                                   buffer + logname.substr(logname.length() - ext_.size());

//            std::cerr << "Switcho su <" << destname << ">\n";

            rename(logname.c_str(), destname.c_str());
        }

        f = fopen(logname.c_str(), "w");
	}


	if (f) {
        fwrite(dst.c_str(), dst.length(), 1, f);

		fclose(f);
	}
}

/*!
  \brief scrive un log con formattazione in stile printf().

  Scrive sui log una stringa usando la stessa sintassi di formattazione di printf(),
  il primo parametro indica il livello di gravita` del messaggio, se il livello di gravita`
  della chiamata e` inferiore a quello settato nella variabile LogLevel il messaggio non
  viene visualizzato, i livelli di gravita' disponibili sono:

  <b>T_D</b> - informazione di debug<br>
  <b>T_I</b> - informazione<br>
  <b>T_W</b> - informazione importante<br>
  <b>T_E</b> - errore<br>
  <b>T_F</b> - errore molto grave<br>

  \param level livello di gravita` del messaggio di log
  \param format stringa di formattazione
  \param  ... parametri da sostituire all'interno della stringa di formattazione

  \return void
*/


#ifndef WIN32
static int syslog_level[] = {LOG_NOTICE, LOG_ERR, LOG_WARNING, LOG_INFO, LOG_DEBUG, LOG_INFO};
#endif

void
Logger::FormatString(const char *file, int line, int level, const std::string &logname)
{
    if (const char *f = strrchr(file, '/'))
        file = f + 1;

    switch (dest_) {
        case LogToNone:
            break;
        case LogToCustom:
            if (cbk_)
                cbk_(file, line, level, Thread::CurrentThreadName(), logbuffer_);
            break;
        case LogToFile:
        case LogToRemote:
            {
#ifndef IGNORE_REMOTE_LOG

                struct tm localtm;
                char datebuffer[100];
                std::ostringstream logline;

                datebuffer[0] = 0;

                if (host_.empty()) {
                    char buffer[256];

                    gethostname(buffer, sizeof(buffer));

                    if (strlen(buffer) > 15)
                        host_.assign(buffer, 15);
                    else
                        host_ = buffer;
                }

                time_t now = time(NULL);

                if (struct tm *t = localtime_r(&now, &localtm)) {
                    ::strftime(datebuffer, sizeof(datebuffer), "%Y-%m-%d %H:%M:%S", t);
                } else {
                    strcpy(datebuffer, "XXXX-XX-XX XX:XX:XX");
                }

                logline   << datebuffer << ';'
                    << errlevel[level] << ';'
                    << Thread::CurrentThreadName() << ';'
                    << file << " | " << line <<  " | "
                    << logbuffer_ << '\n';
                if (dest_ == LogToRemote && !remote_addr_.empty())
                    RemoteLog(logline.str());
                else
                    PrintDebugLog(logname + ext_, logline.str());
#endif

            }
            break;
        case LogToSystem:
            {
#ifndef WIN32
                syslog(syslog_level[level], "%s (%s) %s:%d - %s",
                              errlevel[level], Thread::CurrentThreadName().c_str(),
                              file, line, logbuffer_);
#else
                std::ostringstream os;
                os << errlevel[level] << " (" << Thread::CurrentThreadName() << ") "
                   << file << ':' << line << ' ' << logbuffer_;
                OutputDebugString(os.str().c_str());
#endif
            }
            break;
        case LogToStdout:
            std::cout << errlevel[level] << " (" << Thread::CurrentThreadName() << ") "
                      << file << ':' << line << ' ' << logbuffer_ << std::endl;
            break;
        case LogToStderr:
            std::cerr << errlevel[level] << " (" << Thread::CurrentThreadName() << ") "
                      << file << ':' << line << ' ' << logbuffer_ << '\n';
            break;
    }
}

void Logger::
remote(const std::string &host, unsigned short port) {
    std::unique_lock<std::mutex> lock(cs_);

#ifndef IGNORE_REMOTE_LOG
    remote_addr_ = host;
    remote_port_ = port;
    dest_ = LogToRemote;

    if (socket_ >= 0) {
        closesocket(socket_);
        socket_ = -1;
    }
#endif
}

void Logger::
RemoteLog(const void *line, size_t len)
{
#ifndef IGNORE_REMOTE_LOG
    if (socket_ < 0) {
        memset(&address_, 0, sizeof(address_));
        address_.sin_addr.s_addr = inet_addr(remote_addr_.c_str());
        address_.sin_port = htons(remote_port_);
        address_.sin_family = AF_INET;

        socket_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    }

    if (socket_ >= 0 && address_.sin_addr.s_addr != INADDR_NONE)
#ifndef WIN32
        ::sendto(socket_, line, len, 0L, (sockaddr *)&address_, sizeof(address_));
#else
        ::sendto(socket_, (const char *)line, len, 0L, (sockaddr *)&address_, sizeof(address_));
#endif
#endif
}

void Logger::
RemoteLog(const std::string &line)
{
    RemoteLog(line.c_str(), line.length());
}

void Logger::
trace(const char *file, int line, int level, const std::string &line_str)
{
    std::unique_lock<std::mutex> lock(cs_);
    logbuffer_[sizeof(logbuffer_) - 1] = 0;
    strncpy(logbuffer_, line_str.c_str(), sizeof(logbuffer_) - 1);
    FormatString(file, line, level, name_);
}

void Logger::
trace(const char *file, int line, int level,const char *format, ...)
{
	va_list args;

    if (level <= level_) {
        std::unique_lock<std::mutex> lock(cs_);
        va_start(args, format);
        vsnprintf(logbuffer_, sizeof(logbuffer_), format, args);
        FormatString(file, line, level, name_);
        va_end(args);
    }
}


void
Logger::vtrace(const char *file, int line, int level,const char *format, va_list args)
{
    if (level <= level_) {
        std::unique_lock<std::mutex> lock(cs_);
        vsnprintf(logbuffer_, sizeof(logbuffer_), format, args);
        if (auto *c = strchr(logbuffer_, '\n'))
            *c = 0;
        if (strlen(logbuffer_) > 0)
            FormatString(file, line, level, name_);
    }
}

