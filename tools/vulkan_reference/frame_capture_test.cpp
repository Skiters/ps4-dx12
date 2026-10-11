#include "reference_frames.h"
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>

static void Require(bool value){if(!value)throw std::runtime_error("Frame capture contract failed");}

int main(int argc,char** argv){
    try{
    Require(argc==2);
    const bool capped=std::string(argv[1])=="cap";
    std::filesystem::create_directories("user/log");
    _putenv_s("SHADPS4_FRAME_CAPTURE","frames.enabled");
    {std::ofstream marker("frames.enabled");marker<<"test";}
    Common::GuestFrameTrace::New(); // Initializes the writer before its marker poll.
    std::this_thread::sleep_for(std::chrono::milliseconds(650));
    const unsigned frames=capped?20000:2;
    for(unsigned i=0;i<frames;++i){
        const auto id=Common::GuestFrameTrace::New();
        Common::GuestFrameTrace::Record(id,"submitted",i+1);
        Common::GuestFrameTrace::Record(id,"gpu-complete",i+1);
        if(i%100==0)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::filesystem::remove("frames.enabled");
    std::this_thread::sleep_for(std::chrono::milliseconds(650));
    const auto outside=Common::GuestFrameTrace::New();
    Common::GuestFrameTrace::Record(outside,"gpu-complete",999999);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::ifstream file("user/log/guest-frames.csv");
    std::string line;unsigned starts{},stops{},completed{},caps{},records{};
    while(std::getline(file,line)){
        ++records;
        Require(line.find(",999999")==std::string::npos);
        if(line.find(",capture-start,")!=std::string::npos)++starts;
        if(line.find(",capture-stop,")!=std::string::npos)++stops;
        if(line.find(",gpu-complete,")!=std::string::npos)++completed;
        if(line.find(",storage-cap,")!=std::string::npos)++caps;
    }
    Require(starts==1);
    if(capped){Require(caps==1&&records<50100);}
    else{Require(caps==0&&stops==1&&completed==2);}
    }catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}
}
