#include "efis/core/engine_runtime_store.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
void require(bool condition,const char* message){if(!condition){std::cerr<<"FAILED: "<<message<<'\n';std::exit(EXIT_FAILURE);}}
int main(){const auto root=std::filesystem::temp_directory_path()/"efis-runtime-store-test";std::filesystem::create_directories(root);const auto path=root/"runtime.state";std::filesystem::remove(path);efis::engine::RuntimeStore store{path};require(!store.load(),"absent store must be a clean first start");store.save({"asso-v-jolly-912-ul","SERIAL-TEST",{1,2,3,4},7,1234});const auto loaded=store.load();require(loaded&&loaded->revision==7&&loaded->totals.tach_hours==3,"valid record must survive round trip");{std::ofstream corrupt(path,std::ios::app);corrupt<<"damage";}bool rejected=false;try{static_cast<void>(store.load());}catch(...){rejected=true;}require(rejected,"corrupted state must be rejected");std::filesystem::remove_all(root);std::cout<<"engine runtime store tests passed\n";}
