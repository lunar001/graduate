#include "vedInstanceManager.h"

void VedInstanceManager::Insert(const string & vmName,  VedInstance * ved)
{
    vedManager[vmName] = ved;

}
VedInstance * VedInstanceManager::GetInstance(const string & vmName)
{
    return vedManager[vmName];
}

void VedInstanceManager::Delete(const string & vmName)
{
   // obviously wrong
    vedManager.erase(vmName);
}
