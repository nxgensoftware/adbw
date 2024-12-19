// avahi-browse regex: ^=  (.*?) (ipv4) (.*?)  *(_adb-tls-connect._tcp) local$\n   hostname = \[(.*?)\]$\n   address = \[(.*?)\]\n   port = \[[0-9]*\]$\n   txt = \[(.*?)\]$

#include <cstdlib>
#include <iostream>
#include <regex>
#include <string>
#include <array>

const std::string protocol = "_adb-tls-connect._tcp";

struct Device{
    std::string network_device;
    bool ipv; // 0 = ipv4, 1 = ipv6
    std::string serial;
    std::string avahi_protocol;
    std::string scope;
    std::string hostname;
    std::string address;
    std::string port;
    std::string txt;
};

int test_cmd(const char* cmd){
    std::string test = "which " + std::string(cmd) + " > /dev/null 2>&1";
    return system(test.c_str());
}

// https://stackoverflow.com/a/478960
std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

int main(int argc, char** argv){

    if(test_cmd("avahi-browse") != 0){
        std::cerr << "avahi-browse not found" << std::endl;
        return 1;
    }

    if(test_cmd("adb") != 0){
        std::cerr << "adb not found" << std::endl;
        return 1;
    }

    if(test_cmd("dig") != 0){
        std::cerr << "dig not found" << std::endl;
        return 1;
    }

    std::string avahi_cmd = "avahi-browse -r -t " + protocol;
    std::string avahi_out = exec(avahi_cmd.c_str());

    if(avahi_out.empty()){
        std::cerr << "No devices found" << std::endl;
        return 1;
    }

    std::regex regex(R"(^=  ([^\s]+) ([^\s]+) ([^\s]+) *([^\s]+) ([^\s]+)\n *hostname = \[([^\s]+)\]\n *address = \[([^\s]+)\]\n *port = \[([^\s]+)\]\n *txt = \[\"([^\s]+)\"\]\n)", std::regex_constants::ECMAScript | std::regex_constants::icase | std::regex_constants::multiline);
    std::smatch matches;

    bool result = std::regex_search(avahi_out, matches, regex);

    if(!result){
        std::cerr << "Sum fucky happened with the regex" << std::endl;
        return 1;
    }

    Device device;
    device.network_device = matches[1];
    device.ipv = matches[2] == "IPv4";
    device.serial = matches[3].str().substr(matches[3].str().find('-') + 1, 16);
    device.avahi_protocol = matches[4];
    device.scope = matches[5];
    device.hostname = matches[6];
    device.address = matches[7];
    device.port = matches[8];
    device.txt = matches[9];

    bool mdns = false;

    if(device.scope == "local"){
        // make reverse dns lookup using dig
        std::string dig_command = "dig -x " + device.address + " +short";
        std::string dig_out = exec(dig_command.c_str());

        if(dig_out.empty()){
            std::cerr << "Reverse dns lookup failed" << std::endl;
        } else {
            char c = dig_out[dig_out.length() - 2];
            if(c == '.'){
                dig_out = dig_out.substr(0, dig_out.length() - 2);
            }
            device.hostname = dig_out;
            mdns = true;
        }
    }

    int adb_result = system(("adb connect " + (mdns ? device.hostname : device.address) + ":" + device.port + " > /dev/null 2>&1").c_str());

    if(adb_result != 0){
        std::cerr << "Failed to connect to device" << std::endl;
        return 1;
    }

    std::string friendly_name = mdns ? "" + device.hostname + " [" + device.serial + "]" : device.address + " [" + device.serial + "]";

    std::cout << "Connected to " << friendly_name << " on port " << device.port << std::endl;
    
    return 0;
}