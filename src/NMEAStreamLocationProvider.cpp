/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is DOM Geolocation.
 *
 * The Initial Developer of the Original Code is
 * Doug Turner <dougt@meer.net>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsStringAPI.h"

#include "nsXPCOM.h"
#include "nsIGenericFactory.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch2.h"
#include "NMEAStreamLocationProvider.h"
#include "nsThreadUtils.h"
#include "nsIProxyObjectManager.h"
#include "nmeap.h"
#include "nsIProgrammingLanguage.h"
#include "nsDOMClassInfoID.h"
#include "nsIProxyObjectManager.h"
#include "nsAutoPtr.h"
#include "nsServiceManagerUtils.h"

/**
 * Simple object that holds a single point in space.
 */ 
class nsGeolocation : public nsIDOMGeolocation, public nsIClassInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMGEOLOCATION
  NS_DECL_NSICLASSINFO

  nsGeolocation(double aLat, double aLong, double aAlt, double aHError, double aVError, long long aTimestamp)
    : mLat(aLat), mLong(aLong), mAlt(aAlt), mHError(aHError), mVError(aVError), mTimestamp(aTimestamp){};

private:
  ~nsGeolocation(){}
  double mLat, mLong, mAlt, mHError, mVError;
  long long mTimestamp;
};

////////////////////////////////////////////////////
// nsGeolocation
////////////////////////////////////////////////////
NS_INTERFACE_MAP_BEGIN(nsGeolocation)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeolocation)
  NS_INTERFACE_MAP_ENTRY(nsIDOMGeolocation)
  NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
NS_INTERFACE_MAP_END

NS_IMPL_THREADSAFE_ADDREF(nsGeolocation)
NS_IMPL_THREADSAFE_RELEASE(nsGeolocation)

NS_IMETHODIMP
nsGeolocation::GetLatitude(double *aLatitude)
{
  *aLatitude = mLat;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::GetLongitude(double *aLongitude)
{
  *aLongitude = mLong;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::GetAltitude(double *aAltitude)
{
  *aAltitude = mAlt;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::GetHorizontalAccuracy(double *aHorizontalAccuracy)
{
  *aHorizontalAccuracy = mHError;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::GetVerticalAccuracy(double *aVerticalAccuracy)
{
  *aVerticalAccuracy = mVError;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::GetTimestamp(DOMTimeStamp* aTimestamp)
{
  *aTimestamp = mTimestamp;
  return NS_OK;
}

// nsIClassInfo implementation -- required to be able to create an
// object and pass it back into the content dom.  Only a flags
// implementation is required.
NS_IMETHODIMP
nsGeolocation::GetInterfaces(PRUint32   *aCount,
                             nsIID   * **aArray)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsGeolocation::GetHelperForLanguage(PRUint32 language,
                                    nsISupports **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::GetContractID(char * *aContractID)
{
  *aContractID = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::GetClassDescription(char * *aClassDescription)
{
  *aClassDescription = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::GetClassID(nsCID * *aClassID)
{
  *aClassID = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::GetImplementationLanguage(PRUint32 *aLang)
{
  *aLang = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::GetFlags(PRUint32 *aFlags)
{
  *aFlags = nsIClassInfo::DOM_OBJECT;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
  return NS_ERROR_NOT_AVAILABLE;
}



NS_IMPL_THREADSAFE_ISUPPORTS2(NMEAStreamLocationProvider, nsIGeolocationProvider, nsIRunnable)

NMEAStreamLocationProvider::NMEAStreamLocationProvider()
: mHasSeenLocation(PR_FALSE), mKeepGoing(PR_TRUE)
{
}

NMEAStreamLocationProvider::~NMEAStreamLocationProvider()
{
  mKeepGoing = PR_FALSE;
}

NS_IMETHODIMP NMEAStreamLocationProvider::Startup()
{
#ifdef DEBUG
  printf("@@@@@ gps add paser failed\n");
#endif
  nsresult rv;
  nsCString nmeaPath;
  nsCOMPtr<nsIPrefBranch2> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs)
    rv = prefs->GetCharPref("geo.nmea.path", getter_Copies(nmeaPath));

#ifdef DEBUG
  printf("@@@@@ gps startup\n");
#endif

  if (NS_FAILED(rv))
    return rv;

  mNMEAPath = nmeaPath;
  mKeepGoing = PR_TRUE;

  return NS_NewThread(getter_AddRefs(mThread), this);
}

NS_IMETHODIMP NMEAStreamLocationProvider::IsReady(PRBool *_retval NS_OUTPARAM)
{
#ifdef DEBUG
  printf("@@@@@ gps isready called\n");
#endif

  *_retval = mHasSeenLocation;
  return NS_OK;
}

NS_IMETHODIMP NMEAStreamLocationProvider::Watch(nsIGeolocationUpdate *callback)
{
#ifdef DEBUG
  printf("@@@@@ gps watch called\n");
#endif

  mCallback = callback; // weak ref
  return NS_OK;
}

NS_IMETHODIMP NMEAStreamLocationProvider::GetCurrentLocation(nsIDOMGeolocation * *aCurrentLocation)
{
#ifdef DEBUG
  printf("@@@@@ gps get current location called\n");
#endif

  NS_IF_ADDREF(*aCurrentLocation = mLastLocation);
  return NS_OK;
}

NS_IMETHODIMP NMEAStreamLocationProvider::Shutdown()
{
#ifdef DEBUG
  printf("@@@@@ gps shutdown called\n");
#endif

  mKeepGoing = PR_FALSE;

  if (mThread) {
    // I think most nsIThread do processing of events.  We
    // don't, and get hung up on fread().  Force fread() to
    // return.
    PRThread *thread;
    mThread->GetPRThread(&thread);
    PR_Interrupt(thread);

    mThread->Shutdown();
  }
  return NS_OK;
}

void NMEAStreamLocationProvider::Update(nsIDOMGeolocation* aLocation)
{
#ifdef DEBUG
  printf("@@@@@ gps update called\n");
#endif

  mHasSeenLocation = PR_TRUE;
  mLastLocation = aLocation;

  if (mCallback) {
    nsCOMPtr<nsIGeolocationUpdate> proxiedUpdate;

    nsCOMPtr<nsIProxyObjectManager> proxyObjMgr = do_GetService("@mozilla.org/xpcomproxy;1");
    proxyObjMgr->GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                                   NS_GET_IID(nsIGeolocationUpdate),
                                   mCallback,
                                   NS_PROXY_ASYNC | NS_PROXY_ALWAYS,
                                   getter_AddRefs(proxiedUpdate));
    proxiedUpdate->Update(aLocation);
  }
}

NS_IMETHODIMP NMEAStreamLocationProvider::Run()
{
#ifdef DEBUG
  printf("@@@@@ gps run called\n");
#endif

  // okay, lets do it.  nsLocalFile doesn't open special
  // files on the mac correctly, so lets do this old school.

  nmeap_context_t nmea;
  nmeap_gga_t     gga;

  int status = nmeap_init(&nmea, nsnull);
  if (status != 0) {
    Update(nsnull);
#ifdef DEBUG
    printf("@@@@@ gps init failed\n");
#endif
    return NS_ERROR_FAILURE;
  }

  status = nmeap_addParser(&nmea,"GPGGA", nmeap_gpgga, nsnull, &gga);
  if (status != 0) {
    Update(nsnull);
#ifdef DEBUG
    printf("@@@@@ gps add paser failed\n");
#endif
    return NS_ERROR_FAILURE;
  }
  
  FILE* nmeaStream = fopen(mNMEAPath.get(), "r");

  if (!nmeaStream) {
    Update(nsnull);
#ifdef DEBUG
    printf("@@@@@ gps open failed\n");
#endif
    return NS_ERROR_NOT_AVAILABLE;
  }

  char ch;
  PRTime lastUpdated = 0;

  while (mKeepGoing) {
    
    if (fread(&ch, 1, 1, nmeaStream) != 1)
    {
      Update(nsnull);
#ifdef DEBUG
      printf("@@@@@ gps read failed\n");
#endif
      return NS_ERROR_NOT_AVAILABLE;
    }
  
    status = nmeap_parse(&nmea, ch);
#ifdef DEBUG
    printf("@@@@@ gps parse returned %d\n", status);
#endif
  
    switch(status) {
      case NMEAP_GPGGA:
        {
          // Found a location.  

          // Lets throttle this back a bit:
          PRTime now = PR_Now();
          if (now - lastUpdated > 2000)
          {
#ifdef DEBUG
            printf("@@@@@ gps update (%d. %d)\n", gga.latitude, gga.longitude);
#endif
            lastUpdated = now;
            nsRefPtr<nsGeolocation> somewhere = new nsGeolocation(gga.latitude,
                                                                  gga.longitude,
                                                                  gga.altitude,
                                                                  gga.quality,
                                                                  gga.quality,
                                                                  gga.time);
            Update(somewhere);
          }
        }
    default:
      ;
    }
  }

  fclose(nmeaStream);
  return NS_OK;
}

#define MYFIRSTCOMPONENT_CID \
{ 0x3FF8FB9F, 0xEE63, 0x48DF, { 0x89, 0xF0, 0xDA, 0xCE, 0x02, 0x42, 0xFD, 0x82 } }

NS_GENERIC_FACTORY_CONSTRUCTOR(NMEAStreamLocationProvider)
	
//----------------------------------------------------------
	
static const nsModuleComponentInfo components[] =
{
	{
		"NMEA Geolocation Provider",
		MYFIRSTCOMPONENT_CID,
		NS_GEOLOCATION_PROVIDER_CONTRACTID,
        NMEAStreamLocationProviderConstructor
	},
};
	
NS_IMPL_NSGETMODULE(NMEA_GEOLOCATION_PROVIDER, components)
