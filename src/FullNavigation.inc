// SPDX-License-Identifier: GPL-3.0-or-later
#include "SurfaceMap.h"
inline void Vitaleon::createNavigation(){
    auto specification=Json::parse(surfaceMapJson);
    auto find=[&](std::string name){for(int i=0;i<numParams();++i)if(param(i).name==name)return i;std::replace(name.begin(),name.end(),'_',' ');for(int i=0;i<numParams();++i)if(param(i).name==name)return i;throw std::runtime_error("Unknown surface parameter: "+name);};
    rememberedPages_.fill(-1);
    for(const auto& page:specification){SurfaceView view{page.at("group"),page.at("member"),{}};view.hidden=page.value("hidden",false);
        if(page.at("chart").is_string()){const std::string chart=page.at("chart");const int at=int(surfaceViews_.size());
            if(chart=="mxtable")matrixView_=at;else if(chart=="pickgroup")pickGroupView_=at;else if(chart=="picktarget")pickTargetView_=at;}for(const auto& section:page.at("sections")){std::vector<int> row;for(const auto& item:section.at("items"))row.push_back(find(item));view.sections.push_back(std::move(row));}if(rememberedPages_[view.group]<0&&!view.hidden)rememberedPages_[view.group]=int(surfaceViews_.size());surfaceViews_.push_back(std::move(view));}
    auto integer=[&](const char* id,const char* name,int max){auto p=mpc::intParam(id,name,0,max,0);p.automatable=false;return addParam(p);};
    uiGroup_=integer("ui_group","UI Group",6);uiPage_=integer("ui_page","UI Page",int(surfaceViews_.size())-1);uiSection_=integer("ui_section","UI Section",31);
    uiKnobs_=numParams();for(int i=0;i<16;++i){auto id="ui_knob_"+std::to_string(i),name="UI Control "+std::to_string(i+1);auto p=mpc::floatParam(id.c_str(),name.c_str(),0,1,0);p.automatable=false;addParam(p);uiTargets_[i].store(-1);}
    // Appended after every existing parameter (index 1855). A momentary trigger for
    // the PICK buttons of the matrix rows: the plugin resets it to 0 at once, so
    // the same row can be picked again (a radio button that is already lit sends nothing).
    uiPick_=integer("ui_pick","UI Pick",64);
    updateNavigation();
}
inline void Vitaleon::updateNavigation(){
    int page=mpc::clamp(getInt(uiPage_),0,int(surfaceViews_.size())-1);const auto& v=surfaceViews_[page];publish(uiGroup_,float(v.group));if(!v.hidden)rememberedPages_[v.group]=page;
    int section=mpc::clamp(getInt(uiSection_),0,int(v.sections.size())-1);publish(uiSection_,float(section));
    if(v.group==3)publish(envSelect_,float(v.member));
    if(v.group==4){publish(lfoSelect_,float(v.member));publish(curveTarget_,0);curveSelectionChanged_.store(true);inspectCurve_.store(true);}
    if(v.group==6&&v.member==1){publish(curveTarget_,1);curveSelectionChanged_.store(true);inspectCurve_.store(true);}
    const auto& targets=v.sections[section];for(int i=0;i<16;++i)uiTargets_[i].store(i<int(targets.size())?targets[i]:-1);
    refreshProxies();browserDisplayDirty_.store(true);
}
inline bool Vitaleon::navigationChanged(int index,float plain){
    if(uiGroup_<0)return false;
    if(index>=uiKnobs_&&index<uiKnobs_+16){
        int target=uiTargets_[index-uiKnobs_].load();if(target<0)return true;
        // Record the stable sound parameter, never a page-dependent UI slot.
        int automated=target;
        if(target>=homeControls_&&target<homeControls_+7)automated=homeTarget(target-homeControls_);
        if(target==homeFilter_)automated=controlIndex("filter_1_on");
        if(target==advancedValue_)automated=advancedTargets_[getInt(advancedSelector_)];
        if(target==effectType_)automated=effectOrder_;
        for(const auto& bank:banks_)for(size_t k=0;k<bank.proxies.size();++k)if(target==bank.proxies[k])automated=bank.targets[mpc::clamp(getInt(bank.selector)-1,0,int(bank.targets.size())-1)][k];
        setNormalised(target,plain);if(param(automated).automatable)notifyHostParameterChanged(automated);return true;
    }
    if(index==uiPick_){const int route=int(std::lround(plain));
        if(route>0&&matrixView_>=0&&getInt(uiPage_)==matrixView_){publish(uiPick_,0);set(routeSelect_,float(route));navigateTo(pickGroupView_,0);}
        return true;}
    if(index!=uiGroup_&&index!=uiPage_&&index!=uiSection_)return false;
    std::lock_guard<std::mutex> lock(navigationMutex_);
    if(index==uiGroup_){publish(uiPage_,float(rememberedPages_[mpc::clamp(int(std::lround(plain)),0,6)]));publish(uiSection_,0);}
    // Entering the matrix shows the page of the selected route.
    else if(index==uiPage_)publish(uiSection_,getInt(uiPage_)==matrixView_?float((getInt(routeSelect_)-1)/8):0.0f);
    // Target picker step 1 -> 2: a group button sets the section, the entry view shows that group.
    else if(index==uiSection_&&pickGroupView_>=0&&getInt(uiPage_)==pickGroupView_&&std::lround(plain)>0)publish(uiPage_,float(pickTargetView_));
    updateNavigation();return true;
}
inline void Vitaleon::navigateTo(int page,int section){
    if(page<0||uiPage_<0)return;
    std::lock_guard<std::mutex> lock(navigationMutex_);publish(uiPage_,float(page));publish(uiSection_,float(section));updateNavigation();
}
inline void Vitaleon::refreshNavigation(){if(uiKnobs_>=0)for(int i=0;i<16;++i){int target=uiTargets_[i].load();publishNormalised(uiKnobs_+i,target>=0?getNormalised(target):0);}}
