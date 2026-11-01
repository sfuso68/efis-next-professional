#include "efis/core/engine_runtime_store.hpp"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
namespace efis::engine { namespace {
// Rileva la corruzione accidentale del file, non la sua alterazione
// deliberata: non entra alcun segreto, quindi chi puo' scrivere il file puo'
// anche ricalcolare il valore. Lo stato che l'interfaccia espone in caso di
// mancata corrispondenza va letto con questa portata.
std::uint64_t checksum(std::string_view text) { std::uint64_t value=14695981039346656037ULL; for (const unsigned char byte:text) { value^=byte; value*=1099511628211ULL; } return value; }

// Il formato e' a righe `chiave=valore` e non avrebbe modo di rappresentare un
// ritorno a capo dentro un valore. Senza codifica un valore che ne contiene uno
// non viene soltanto troncato: le righe che lo seguono vengono lette come
// campi, e poiche' `profile_id` ed `engine_serial` sono scritti prima dei
// totali, un campo iniettato precederebbe quello reale e vincerebbe. Il
// checksum non potrebbe accorgersene, perche' viene calcolato sul corpo che
// contiene gia' l'iniezione. Si codifica quindi in scrittura, non si confida
// nel fatto che il chiamante non scriva mai un ritorno a capo.
std::string encode(std::string_view value) {
    std::string out; out.reserve(value.size());
    for (const char character : value) {
        switch (character) {
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            default: out.push_back(character);
        }
    }
    return out;
}

// Una sequenza di escape sconosciuta o troncata non si interpreta a intuito: un
// file di stato che non si legge in modo univoco viene rifiutato.
std::string decode(std::string_view value) {
    std::string out; out.reserve(value.size());
    for (std::size_t index=0; index<value.size(); ++index) {
        if (value[index]!='\\') { out.push_back(value[index]); continue; }
        if (++index==value.size()) throw std::runtime_error("runtime state ends with a dangling escape");
        switch (value[index]) {
            case '\\': out.push_back('\\'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            default: throw std::runtime_error("runtime state has an unknown escape");
        }
    }
    return out;
}

std::string payload(const RuntimeRecord& r) { std::ostringstream out; out<<"schema=1\nprofile_id="<<encode(r.profile_id)<<"\nengine_serial="<<encode(r.engine_serial)<<"\nmaster_hours="<<std::setprecision(17)<<r.totals.master_hours<<"\nengine_hours="<<r.totals.engine_hours<<"\ntach_hours="<<r.totals.tach_hours<<"\nflight_hours="<<r.totals.flight_hours<<"\nrevision="<<r.revision<<"\nsaved_wall_ms="<<r.saved_wall_ms<<'\n'; return out.str(); }

// I quattro contatori sono accumulatori che partono dai valori iniziali del
// profilo e non tornano indietro: un totale negativo o non finito descrive un
// file guasto, non un motore. Vale in lettura e in scrittura, perche' un `nan`
// scritto sul file verrebbe riletto da `stod` senza errore.
void require_plausible(const RuntimeTotals& totals) {
    for (const double value : {totals.master_hours, totals.engine_hours,
                               totals.tach_hours, totals.flight_hours}) {
        if (!std::isfinite(value) || value<0.0) throw std::runtime_error("runtime state has an implausible total");
    }
}

double total(const std::unordered_map<std::string,std::string>& fields, const char* key) {
    const double value=std::stod(fields.at(key));
    if (!std::isfinite(value)||value<0.0) throw std::runtime_error("runtime state has an implausible total");
    return value;
}

std::uint64_t counter(const std::unordered_map<std::string,std::string>& fields, const char* key) {
    const auto& text=fields.at(key);
    // `stoull` accetta un segno e lo avvolge: un valore negativo diventerebbe
    // un contatore enorme invece di un errore.
    if (text.empty()||text.front()=='-'||text.front()=='+') throw std::runtime_error("runtime state has a malformed counter");
    return std::stoull(text);
}
}
std::optional<RuntimeRecord> RuntimeStore::load() const {
    std::ifstream input(path_,std::ios::binary); if(!input)return std::nullopt; std::ostringstream bytes; bytes<<input.rdbuf(); const std::string all=bytes.str();
    // Il checksum e' l'ultima riga, quindi si cerca il suo inizio di riga e non
    // la sequenza in mezzo al testo: un valore potrebbe contenerla.
    static constexpr std::string_view tag="\nchecksum=";
    std::size_t body_length=0, encoded_start=0;
    if (const auto marker=all.rfind(tag); marker!=std::string::npos) { body_length=marker+1; encoded_start=marker+tag.size(); }
    else if (all.rfind("checksum=",0)==0) { body_length=0; encoded_start=9; }
    else throw std::runtime_error("runtime checksum missing");
    const std::string body=all.substr(0,body_length);
    std::string encoded=all.substr(encoded_start);if(!encoded.empty()&&encoded.back()=='\n')encoded.pop_back();if(encoded.size()!=16)throw std::runtime_error("runtime checksum malformed");
    std::uint64_t expected{}; std::istringstream check(encoded); check>>std::hex>>expected; if(!check||check.peek()!=std::istringstream::traits_type::eof()||expected!=checksum(body)) throw std::runtime_error("runtime checksum mismatch");
    // Una chiave ripetuta non stabilisce una precedenza implicita: e' un errore.
    // `emplace` conserverebbe la prima occorrenza, cioe' esattamente quella che
    // un'iniezione avrebbe inserito prima del campo reale.
    std::unordered_map<std::string,std::string> f; std::istringstream lines(body); for(std::string line;std::getline(lines,line);){const auto split=line.find('=');if(split==std::string::npos)throw std::runtime_error("runtime state has a line without a field");if(!f.emplace(line.substr(0,split),line.substr(split+1)).second)throw std::runtime_error("runtime state repeats a field");}
    if(f.at("schema")!="1")throw std::runtime_error("unsupported runtime schema");
    RuntimeRecord r; r.profile_id=decode(f.at("profile_id"));r.engine_serial=decode(f.at("engine_serial"));
    r.totals.master_hours=total(f,"master_hours");r.totals.engine_hours=total(f,"engine_hours");r.totals.tach_hours=total(f,"tach_hours");r.totals.flight_hours=total(f,"flight_hours");
    r.revision=counter(f,"revision");r.saved_wall_ms=counter(f,"saved_wall_ms");return r;
}
void RuntimeStore::save(const RuntimeRecord& r) const {
    require_plausible(r.totals);
    if(path_.has_parent_path()) {
        std::filesystem::create_directories(path_.parent_path());
    }
    const auto temporary=path_.string()+".new";
    const auto backup=path_.string()+".bak";
    const std::string body=payload(r);
    {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("cannot open runtime temporary file");out<<body<<"checksum="<<std::hex<<std::setw(16)<<std::setfill('0')<<checksum(body)<<'\n';out.flush();if(!out)throw std::runtime_error("cannot write runtime temporary file");}
    std::error_code error;std::filesystem::remove(backup,error);error.clear();if(std::filesystem::exists(path_)){std::filesystem::rename(path_,backup,error);if(error)throw std::runtime_error("cannot rotate runtime backup");}std::filesystem::rename(temporary,path_,error);if(error){std::error_code restore;if(std::filesystem::exists(backup))std::filesystem::rename(backup,path_,restore);throw std::runtime_error("cannot activate runtime state");}
}
}
