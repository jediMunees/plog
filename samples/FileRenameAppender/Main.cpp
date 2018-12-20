//
// FileRenameAppender - shows how to implement a custom appender that renames log file and continue logging with current name.
//

#include <plog/Log.h>
#include <plog/Appenders/IAppender.h>
#include <plog/Converters/UTF8Converter.h>
#include <plog/Util.h>
#include <list>
#include <algorithm>

namespace plog
{
    template<class Formatter, class Converter = UTF8Converter>
    class FileRenameAppender : public IAppender
    {
    public:
        FileRenameAppender(const util::nchar* fileName, size_t maxFileSize = 0, int maxFiles = 0)
            : m_fileSize()
            , m_maxFileSize((std::max)(static_cast<off_t>(maxFileSize), static_cast<off_t>(1000))) // set a lower limit for the maxFileSize
            , m_maxFiles(maxFiles)
            , m_firstWrite(true)
        {
            util::splitFileName(fileName, m_fileNameNoExt, m_fileExt);
        }

    #ifdef _WIN32
        FileRenameAppender(const char* fileName, size_t maxFileSize = 0, int maxFiles = 0)
            : m_fileSize()
            , m_maxFileSize((std::max)(static_cast<off_t>(maxFileSize), static_cast<off_t>(1000))) // set a lower limit for the maxFileSize
            , m_maxFiles(maxFiles)
            , m_firstWrite(true)
        {
            util::splitFileName(util::toWide(fileName).c_str(), m_fileNameNoExt, m_fileExt);
        }
    #endif

        virtual void write(const Record& record)
        {
            util::MutexLock lock(m_mutex);

            if (m_firstWrite)
            {
                openLogFile();
                m_firstWrite = false;
            }
            else if (m_maxFiles > 0 && m_fileSize > m_maxFileSize && -1 != m_fileSize)
            {
                rollLogFiles();
            }

            int bytesWritten = m_file.write(Converter::convert(Formatter::format(record)));

            if (bytesWritten > 0)
            {
                m_fileSize += bytesWritten;
            }
        }

        void rollLogFiles()
        {
            m_file.close();

            util::nstring lastFileName = buildFileName(m_maxFiles - 1);
            util::File::unlink(lastFileName.c_str());

            for (int fileNumber = m_maxFiles - 2; fileNumber >= 0; --fileNumber)
            {
                util::nstring currentFileName = buildFileName(fileNumber);
                util::nstring nextFileName = buildFileName(fileNumber + 1);

                util::File::rename(currentFileName.c_str(), nextFileName.c_str());
            }

            openLogFile();
            m_firstWrite = false;
        }

        void backupLogfile(const char* newFilename)
        {
            util::MutexLock lock(m_mutex);

            util::nstring currentFilename = buildFileName();

            m_file.close();

            util::File::unlink(newFilename);
            util::File::rename(currentFilename.c_str(), newFilename);

            openLogFile();
            m_firstWrite = false;
        }

    private:
        void openLogFile()
        {
            util::nstring fileName = buildFileName();
            m_fileSize = m_file.open(fileName.c_str());

            if (0 == m_fileSize)
            {
                int bytesWritten = m_file.write(Converter::header(Formatter::header()));

                if (bytesWritten > 0)
                {
                    m_fileSize += bytesWritten;
                }
            }
        }

        util::nstring buildFileName(int fileNumber = 0)
        {
            util::nostringstream ss;
            ss << m_fileNameNoExt;

            if (fileNumber > 0)
            {
                ss << '.' << fileNumber;
            }

            if (!m_fileExt.empty())
            {
                ss << '.' << m_fileExt;
            }

            return ss.str();
        }

    private:
        util::Mutex     m_mutex;
        util::File      m_file;
        off_t           m_fileSize;
        const off_t     m_maxFileSize;
        const int       m_maxFiles;
        util::nstring   m_fileExt;
        util::nstring   m_fileNameNoExt;
        bool            m_firstWrite;
    };
}


int main()
{
    static plog::FileRenameAppender<plog::TxtFormatter> fileRenameAppender("Test.log", 2000000, 1); // Create our custom appender.
    plog::init(plog::debug, &fileRenameAppender); // Initialize the logger with our appender.

    LOGD << "A debug message from scenario_1";
    LOGD << "Test pass from scenario_1";
    fileRenameAppender.backupLogfile("scenario_1.log");

    LOGD << "A new debug message from scenario_2";
    LOGD << "Test pass from scenario_2";
    fileRenameAppender.backupLogfile("scenario_2.log");

    return 0;
}
