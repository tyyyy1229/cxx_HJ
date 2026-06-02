
#include "common_setup.hpp"

static bhc::bhcInit init;

template<bool O3D, bool R3D> int mainmain()
{
    bhc::bhcParams<O3D> params;
    bhc::bhcOutputs<O3D, R3D> outputs;
    if(!bhc::setup<O3D, R3D>(init, params, outputs)) return 1;
    if(!bhc::run<O3D, R3D>(params, outputs)) return 1;
    if(!bhc::writeout<O3D, R3D>(params, outputs, nullptr)) return 1;
    bhc::finalize<O3D, R3D>(params, outputs);
    return 0;
}

void showhelp(const char *argv0)
{
    

}

int main(int argc, char **argv)
{
    int dimmode = BHC_DIM_ONLY;
    std::string FileRoot;
    for(int32_t i = 1; i < argc; ++i) {
        std::string s = argv[i];
        if(argv[i][0] == '-') {
            if(s.length() >= 2 && argv[i][1] == '-') { // two dashes
                s = s.substr(1);
            }
            if(s == "-1" || s == "-singlethread") {
                init.numThreads = 1;
            } else if(s == "-2" || s == "-2D") {
                dimmode = 2;
            } else if(s == "-Nx2D" || s == "-2D3D" || s == "-2.5D" || s == "-4") {
                dimmode = 4;
            } else if(s == "-3" || s == "-3D") {
                dimmode = 3;
            } else if(s == "-copy" || s == "-raycopy") {
                init.useRayCopyMode = true;
            } else if(s == "-?" || s == "-h" || s == "-help") {
                showhelp(argv[0]);
                return 0;
            } else {
                size_t equalspos = s.find("=");
                if(equalspos == std::string::npos) {
                    std::cout << "Unknown command-line option \"" << s << "\", try "
                              << argv[0] << " --help\n";
                    return 1;
                }
                std::string key   = s.substr(0, equalspos);
                std::string value = s.substr(equalspos + 1);
                if(key == "-gpu" || key == "-device") {
                    if(!bhc::isInt(value, false)) {
                        std::cout << "Value \"" << value
                                  << "\" for --gpu argument is invalid, try " << argv[0]
                                  << " --help\n";
                        return 1;
                    }
                    init.gpuIndex = std::stoi(value);
                } else if(key == "-mem" || key == "-memory") {
                    size_t multiplier = 1u;
                    size_t base       = 1000u;
                    if(bhc::endswith(value, "B") || bhc::endswith(value, "b")) {
                        value = value.substr(0, value.length() - 1);
                    }
                    if(bhc::endswith(value, "i")) {
                        base  = 1024u;
                        value = value.substr(0, value.length() - 1);
                    }
                    if(bhc::endswith(value, "k") || bhc::endswith(value, "K")) {
                        multiplier = base;
                        value      = value.substr(0, value.length() - 1);
                    } else if(bhc::endswith(value, "M")) {
                        multiplier = base * base;
                        value      = value.substr(0, value.length() - 1);
                    } else if(bhc::endswith(value, "G")) {
                        multiplier = base * base * base;
                        value      = value.substr(0, value.length() - 1);
                    }
                    if(!bhc::isInt(value, false) || (base == 1024u && multiplier == 1u)) {
                        std::cout << "Value \"" << value
                                  << "\" for --memory argument is invalid, try "
                                  << argv[0] << " --help\n";
                        return 1;
                    }
                    init.maxMemory = multiplier * std::stoi(value);
                } else {
                    std::cout << "Unknown command-line option \"-" << key << "=" << value
                              << "\", try " << argv[0] << " --help\n";
                    return 1;
                }
            }
        } else {
            if(FileRoot.empty()) {
                FileRoot = s;
            } else {
                std::cout << "Intepreting both \"" << FileRoot << "\" and \"" << s
                          << "\" as FileRoot, error\n";
                return 1;
            }
        }
    }
    if(FileRoot.empty()) {
        std::cout << "Must provide FileRoot as command-line parameter, try " << argv[0]
                  << " --help\n";
        return 1;
    }
    init.FileRoot = FileRoot.c_str();

#if BHC_DIM_ONLY > 0
    if(dimmode != BHC_DIM_ONLY) {
        std::cout << "This version of " BHC_PROGRAMNAME " was compiled to only support ";
        if constexpr(BHC_DIM_ONLY == 4) {
            std::cout << "Nx2D";
        } else {
            std::cout << BHC_DIM_ONLY << "D";
        }
        std::cout << " runs\n";
        return 1;
    }
#else
    if(dimmode < 2 || dimmode > 4) {
        std::cout << "No dimensionality specified (--2D, --Nx2D, --3D), assuming 2D\n";
        dimmode = 2;
    }
#endif

    if(dimmode == 2) {
#if BHC_ENABLE_2D
        return mainmain<false, false>();
#else
        std::cout << "This version of " BHC_PROGRAMNAME
                     " was compiled with 2D support disabled\n";
#endif
    }
    if(dimmode == 3) {
#if BHC_ENABLE_3D
        return mainmain<true, true>();
#else
        std::cout << "This version of " BHC_PROGRAMNAME
                     " was compiled with 3D support disabled\n";
#endif
    }
    if(dimmode == 4) {
#if BHC_ENABLE_NX2D
        return mainmain<true, false>();
#else
        std::cout << "This version of " BHC_PROGRAMNAME
                     " was compiled with Nx2D support disabled\n";
#endif
    }
    return 1;
}
