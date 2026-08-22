#include "efis/core/engine_runtime_store.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
namespace efis::engine { namespace {
std::uint64_t checksum(std::string_view text) { std::uint64_t value=14695981039346656037ULL; for (const unsigned char byte:text) { value^=byte; value*=1099511628211ULL; } return value; }
std::string payload(const RuntimeRecord& r) { std::ostringstream out; out<<"schema=1\nprofile_id="<<r.profile_id<<"\nengine_serial="<<r.engine_serial<<"\nmaster_hours="<<std::setprecision(17)<<r.totals.master_hours<<"\nengine_hours="<<r.totals.engine_hours<<"\ntach_hours="<<r.totals.tach_hours<<"\nflight_hours="<<r.totals.flight_hours<<"\nrevision="<<r.revision<<"\nsaved_wall_ms="<<r.saved_wall_ms<<'\n'; return out.str(); }
}
std::optional<RuntimeRecord> RuntimeStore::load() const {
    std::ifstream input(path_,std::ios::binary); if(!input)return std::nullopt; std::ostringstream bytes; bytes<<input.rdbuf(); const std::string all=bytes.str(); const auto marker=all.rfind("checksum="); if(marker==std::string::npos)throw std::runtime_error("runtime checksum missing"); const std::string body=all.substr(0,marker); std::string encoded=all.substr(marker+9);if(!encoded.empty()&&encoded.back()=='\n')encoded.pop_back();if(encoded.size()!=16)throw std::runtime_error("runtime checksum malformed");std::uint64_t expected{}; std::istringstream check(encoded); check>>std::hex>>expected; if(!check||expected!=checksum(body))throw std::runtime_error("runtime checksum mismatch");
    std::unordered_map<std::string,std::string> f; std::istringstream lines(body); for(std::string line;std::getline(lines,line);){const auto split=line.find('=');if(split!=std::string::npos)f.emplace(line.substr(0,split),line.substr(split+1));} if(f.at("schema")!="1")throw std::runtime_error("unsupported runtime schema"); RuntimeRecord r; r.profile_id=f.at("profile_id");r.engine_serial=f.at("engine_serial");r.totals.master_hours=std::stod(f.at("master_hours"));r.totals.engine_hours=std::stod(f.at("engine_hours"));r.totals.tach_hours=std::stod(f.at("tach_hours"));r.totals.flight_hours=std::stod(f.at("flight_hours"));r.revision=std::stoull(f.at("revision"));r.saved_wall_ms=std::stoull(f.at("saved_wall_ms"));return r;
}
void RuntimeStore::save(const RuntimeRecord& r) const {
    if(path_.has_parent_path())std::filesystem::create_directories(path_.parent_path()); const auto temporary=path_.string()+".new";const auto backup=path_.string()+".bak";const std::string body=payload(r);{std::ofstream out(temporary,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("cannot open runtime temporary file");out<<body<<"checksum="<<std::hex<<std::setw(16)<<std::setfill('0')<<checksum(body)<<'\n';out.flush();if(!out)throw std::runtime_error("cannot write runtime temporary file");}
    std::error_code error;std::filesystem::remove(backup,error);error.clear();if(std::filesystem::exists(path_)){std::filesystem::rename(path_,backup,error);if(error)throw std::runtime_error("cannot rotate runtime backup");}std::filesystem::rename(temporary,path_,error);if(error){std::error_code restore;if(std::filesystem::exists(backup))std::filesystem::rename(backup,path_,restore);throw std::runtime_error("cannot activate runtime state");}
}
}
