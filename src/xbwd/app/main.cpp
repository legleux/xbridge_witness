#include <xbwd/app/App.h>
#include <xbwd/app/BuildInfo.h>
#include <xbwd/app/Config.h>
#include <xbwd/rpc/RPCCall.h>

#include <ripple/basics/StringUtilities.h>
#include <ripple/basics/base64.h>
#include <ripple/beast/core/CurrentThreadName.h>
#include <ripple/beast/core/SemanticVersion.h>
#include <ripple/beast/unit_test.h>
#include <ripple/beast/unit_test/dstream.h>
#include <ripple/beast/utility/Journal.h>
#include <ripple/json/json_reader.h>
#include <ripple/json/json_value.h>
#include <ripple/protocol/jss.h>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/program_options.hpp>

#include <exception>
#include <fstream>
#include <stdexcept>

#ifdef BOOST_MSVC
#include <Windows.h>
#endif

static const std::unordered_map<std::string, beast::severities::Severity>
    g_severityMap{
        {"All", beast::severities::kAll},
        {"Trace", beast::severities::kTrace},
        {"Debug", beast::severities::kDebug},
        {"Info", beast::severities::kInfo},
        {"Warning", beast::severities::kWarning},
        {"Error", beast::severities::kError},
        {"Fatal", beast::severities::kFatal},
        {"Disabled", beast::severities::kDisabled},
        {"None", beast::severities::kNone}};

void
printHelp(const boost::program_options::options_description& desc)
{
    std::cerr << xbwd::build_info::serverName
              << " [options] <command> [<argument> ...]\n"
              << desc << std::endl
              << "Commands: \n"
                 "     server_info      Server state info.\n"
                 "     stop             Server stop.\n";
}

static int
runUnitTests()
{
    using namespace beast::unit_test;
    beast::unit_test::dstream dout{std::cout};
    reporter r{dout};
    bool const anyFailed = r.run_each(global_suites());
    if (anyFailed)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

int
main(int argc, char** argv)
{
    namespace po = boost::program_options;

    po::variables_map vm;

    // Set up option parsing.
    //
    po::options_description general("General Options");
    general.add_options()("help,h", "Display this message.")(
        "conf", po::value<std::string>(), "Specify the config file.")(
        "json", po::value<std::string>(), "Handle the provided json request")(
        "quiet,q", "quiet")("silent", "log to file only")(
        "verbose,v", "verbose")("unittest,u", "Perform unit tests.")(
        "version", "Display the build version.");

    po::options_description hidden("Hidden Options");
    hidden.add_options()("cmd", "Command");

    po::positional_options_description posOpt;
    posOpt.add("cmd", -1);

    po::options_description cmdline_options;
    cmdline_options.add(general).add(hidden);
    po::options_description visible("Allowed options");
    visible.add(general);

    // Parse options, if no error.
    try
    {
        po::store(
            po::command_line_parser(argc, argv)
                .options(cmdline_options)
                .positional(posOpt)
                .run(),
            vm);
        po::notify(vm);  // Invoke option notify functions.
    }
    catch (std::exception const&)
    {
        std::cerr << xbwd::build_info::serverName
                  << ": Incorrect command line syntax." << std::endl;
        std::cerr << "Use '--help' for a list of options." << std::endl;
        return EXIT_FAILURE;
    }

    if (vm.count("unittest"))
        return runUnitTests();

    if (vm.count("version"))
    {
        std::cout << xbwd::build_info::serverName << " version "
                  << xbwd::build_info::getVersionString() << std::endl;
        return 0;
    }

    if (vm.count("help"))
    {
        printHelp(visible);
        return EXIT_SUCCESS;
    }

    try
    {
        std::unique_ptr<xbwd::config::Config> config = [&]() {
            auto const configFile = [&]() -> std::string {
                if (vm.count("conf"))
                    return vm["conf"].as<std::string>();
                throw std::runtime_error("must specify a config file");
            }();

            if (!boost::filesystem::exists(configFile))
                throw std::runtime_error("config file does not exist");

            Json::Value jv;
            std::ifstream f;
            f.open(configFile);
            if (!Json::Reader().parse(f, jv))
                throw std::runtime_error("config file contains invalid json");
            return std::make_unique<xbwd::config::Config>(jv);
        }();

        if (vm.count("silent"))
        {
            config->logSilent = true;
        }
        if (config->logSilent && config->logFile.empty())
        {
            std::cerr << "Silent flag present but config missed log file. "
                         "Silent ignored."
                      << std::endl;
            config->logSilent = false;
        }

        auto const logLevel = [&]() -> beast::severities::Severity {
            using namespace beast::severities;

            if (vm.count("quiet"))
                return kFatal;
            else if (vm.count("verbose"))
                return kTrace;
            else if (!config->logLevel.empty())
            {
                const auto i = g_severityMap.find(config->logLevel);
                if (i != g_severityMap.end())
                    return i->second;
            }
            return kInfo;
        }();

        if (vm.count("json") || vm.count("cmd"))
        {
            // do not show logs to console, only raw response from server
            config->logSilent = true;

            using namespace std::literals;
            beast::setCurrentThreadName(
                xbwd::build_info::serverName + ": rpc"s);

            Json::Value jv;
            if (vm.count("json"))
            {
                auto reqStr = vm["json"].as<std::string>();
                if (!reqStr.empty() && reqStr[0] != '{' && reqStr[0] != '"')
                {
                    reqStr = '"' + reqStr + '"';
                }

                if (!Json::Reader().parse(reqStr, jv))
                {
                    std::cerr << "Error: Could not parse json command";
                    return EXIT_FAILURE;
                }
                if (jv.isString())
                {
                    Json::Value o;
                    o[ripple::jss::method] = jv;
                    jv = o;
                }
            }
            else
            {
                auto const cmd = vm["cmd"].as<std::string>();
                if ((cmd == "stop") || (cmd == "server_info"))
                {
                    jv[ripple::jss::method] = cmd;
                }
                else
                {
                    std::cerr << "Error: Invalid command";
                    return EXIT_FAILURE;
                }
            }

            jv[ripple::jss::api_version] =
                xbwd::rpc_call::apiMaximumSupportedVersion;

            auto [ret, res] =
                xbwd::rpc_call::fromCommandLine(*config, jv, logLevel);
            std::cout << res.toStyledString() << std::endl;
            return ret;
        }

        xbwd::App app(std::move(config), logLevel);
        if (!app.setup())
            return EXIT_FAILURE;

        app.start();
        app.run();
    }
    catch (std::exception const& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
