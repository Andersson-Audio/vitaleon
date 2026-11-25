// SPDX-License-Identifier: GPL-3.0-or-later
// Read-only, bounded directory enumeration. Called only by the service thread.
#pragma once
#include <filesystem>
#include <fstream>
#include <sstream>
#include <set>
#include <algorithm>
#include <vector>
#include <string>
#include <stdexcept>
namespace vitaleon_storage {
namespace fs=std::filesystem;
// program >= 0: a sound inside the bank that is open in the browser (index into that bank's catalog).
// kind: a file the browser imports straight into the sound (0.3.25): 1 wavetable (.vitaltable),
// 2 WAV (wavetable or sample, the page's target decides), 3 LFO shape (.vitallfo).
struct Entry {std::string name,path;bool directory=true,sound=false;int program=-1;int kind=0;};
inline std::string unescapeMount(const std::string& s){
    std::string out;for(size_t i=0;i<s.size();++i){if(s[i]=='\\'&&i+3<s.size()&&s[i+1]>='0'&&s[i+1]<='7'&&s[i+2]>='0'&&s[i+2]<='7'&&s[i+3]>='0'&&s[i+3]<='7'){out+=char((s[i+1]-'0')*64+(s[i+2]-'0')*8+s[i+3]-'0');i+=3;}else out+=s[i];}return out;
}
inline bool inside(const fs::path& child,const fs::path& root){auto a=child.begin(),b=root.begin();for(;b!=root.end();++b,++a)if(a==child.end()||*a!=*b)return false;return true;}
// Order of the drive list. The page shows six rows; on the Live II a USB stick was
// the seventh entry, behind the internal partitions, and looked missing (user
// report, 0.3.18). Removable media (FAT/exFAT/NTFS file systems) now come right
// after Plugin Content, internal partitions last.
inline int drivePriority(const std::string& filesystem){
    for(const char* removable:{"vfat","exfat","msdos","ntfs","ntfs3","fuseblk","hfsplus","udf","iso9660"})if(filesystem==removable)return 1;
    return 2;
}
// One line of /proc/self/mountinfo: "id parent dev root mountpoint options... - fstype source superoptions"
inline bool parseMount(const std::string& line,std::string& path,std::string& filesystem){
    std::istringstream row(line);std::string id,parent,dev,mountRoot;if(!(row>>id>>parent>>dev>>mountRoot>>path))return false;
    path=unescapeMount(path);filesystem.clear();std::string field;
    while(row>>field)if(field=="-"){row>>filesystem;break;}
    return true;
}
// sounds: the folder of the sounds saved with SAVE SOUND (0.3.45), listed when it exists.
inline std::vector<Entry> discover(const std::string& pluginFolder,const std::string& sounds=std::string(),const std::string& tables=std::string()){
    struct Found{int priority;Entry entry;};
    std::vector<Found> found;std::set<std::string> seen;
    auto add=[&](fs::path p,std::string name,int priority){std::error_code ec;p=fs::canonical(p,ec);if(ec||!fs::is_directory(p,ec)||ec||!seen.insert(p.string()).second)return;found.push_back({priority,{name,p.string(),true}});};
    add(fs::path(pluginFolder)/"Content","Plugin Content",0);
    if(!sounds.empty())add(fs::path(sounds),"My sounds",0);
    if(!tables.empty())add(fs::path(tables),"User wavetables",0);   // 0.3.53: listed once the folder exists
#ifdef _WIN32
    fs::path folder(pluginFolder);
    for(auto p=folder;!p.empty()&&p!=p.parent_path();p=p.parent_path())if(p.filename()=="VST2 Plugins")add(p.parent_path(),p.parent_path().filename().string()+" (plugin drive)",2);
    for(char drive='A';drive<='Z';++drive){std::string path=std::string(1,drive)+":/";std::error_code ec;if(fs::is_directory(path,ec))add(path,"Drive "+std::string(1,drive)+":",2);}
#else
    // Discover actual mount points, never infer USB/SSD type from device order.
    {std::ifstream mounts("/proc/self/mountinfo");std::string line,path,filesystem;
     while(std::getline(mounts,line)){if(!parseMount(line,path,filesystem))continue;
        if(path.rfind("/media/",0)==0||path.rfind("/run/media/",0)==0||path.rfind("/mnt/",0)==0)add(path,fs::path(path).filename().string(),drivePriority(filesystem));}}
    fs::path folder(pluginFolder);
    for(auto p=folder;!p.empty()&&p!=p.parent_path();p=p.parent_path())if(p.filename()=="VST2 Plugins")add(p.parent_path(),p.parent_path().filename().string()+" (plugin drive)",2);
    // Some hosts expose their user storage through bind-mounted directories.
    for(const char* base:{"/media","/run/media","/mnt"}){std::error_code ec;for(fs::directory_iterator it(base,fs::directory_options::skip_permission_denied,ec),end;!ec&&it!=end;it.increment(ec))if(it->is_directory(ec))add(it->path(),it->path().filename().string(),2);}
#endif
    std::stable_sort(found.begin(),found.end(),[](const Found& a,const Found& b){return a.priority<b.priority;});
    std::vector<Entry> result;for(auto& f:found)result.push_back(std::move(f.entry));
    return result;
}
inline std::vector<Entry> list(const std::string& path,const std::string& boundary){
    std::error_code ec;auto root=fs::canonical(boundary,ec);if(ec)throw std::runtime_error("Drive unavailable. Return to Drives.");
    auto dir=fs::canonical(path,ec);if(ec||!inside(dir,root))throw std::runtime_error("Folder unavailable. Return to Drives.");
    fs::directory_iterator it(dir,ec),end;if(ec)throw std::runtime_error("Cannot read folder. Choose another folder.");
    std::vector<Entry> result;
    for(;it!=end;it.increment(ec)){
        if(ec)throw std::runtime_error("Drive removed or folder unreadable.");
        if(it->is_symlink(ec)||ec){ec.clear();continue;}bool folder=it->is_directory(ec);if(ec){ec.clear();continue;}
        auto ext=it->path().extension().string();for(auto& c:ext)c=char(std::tolower((unsigned char)c));
        const int kind=folder?0:ext==".vitaltable"?1:ext==".wav"?2:ext==".vitallfo"?3:0;
        if(!folder&&!kind&&ext!=".vitalbank"&&ext!=".zip"&&ext!=".vital")continue;
        if(!folder&&!it->is_regular_file(ec))continue;
        if(result.size()>=4096)throw std::runtime_error("Over 4096 items. Use a smaller folder.");
        result.push_back({it->path().filename().string(),it->path().string(),folder,!folder&&ext==".vital",-1,kind});
    }
    std::sort(result.begin(),result.end(),[](const Entry& a,const Entry& b){return a.directory!=b.directory?a.directory>b.directory:a.name<b.name;});return result;
}
}
