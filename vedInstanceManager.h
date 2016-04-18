#ifndef _H_VEDINSTANCEMANAGER_H_
#define _H_VEDINSTANCEMANAGER_H_

#include <iostream>
#include <map>

#include "vedInstance.h"

class VedInstanceManager
{
public:
    void Insert(const string & vmName, VedInstance * ved);
    VedInstance * GetInstance(const string & vmName);
    void Delete(const string & vmName);
private:
    map<string,  VedInstance *> vedManager;

};
#endif
