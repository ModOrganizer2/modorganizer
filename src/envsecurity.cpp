#include "envsecurity.h"
#include "env.h"
#include "envmodule.h"
#include <utility.h>
#include <log.h>

#include <Wbemidl.h>
#include <wscapi.h>
#include <comdef.h>
#include <netfw.h>
#pragma comment(lib, "Wbemuuid.lib")

#include <accctrl.h>
#include <aclapi.h>
#include <sddl.h>
#pragma comment(lib, "advapi32.lib")

namespace env
{

using namespace MOBase;

class WMI
{
public:
  class failed {};

  WMI(const std::string& ns)
  {
    try
    {
      createLocator();
      createService(ns);
      setSecurity();
    }
    catch(failed&)
    {
    }
  }

  template <class F>
  void query(const std::string& q, F&& f)
  {
    if (!m_locator || !m_service) {
      return;
    }

    auto enumerator = getEnumerator(q);
    if (!enumerator) {
      return;
    }

    for (;;)
    {
      COMPtr<IWbemClassObject> object;

      {
        IWbemClassObject* rawObject = nullptr;
        ULONG count = 0;
        auto ret = enumerator->Next(WBEM_INFINITE, 1, &rawObject, &count);

        if (count == 0 || !rawObject) {
          break;
        }

        if (FAILED(ret)) {
          log::error("enum->next() failed, {}", formatSystemMessage(ret));
          break;
        }

        object.reset(rawObject);
      }

      f(object.get());
    }
  }

private:
  COMPtr<IWbemLocator> m_locator;
  COMPtr<IWbemServices> m_service;

  void createLocator()
  {
    void* rawLocator = nullptr;

    const auto ret = CoCreateInstance(
      CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
      IID_IWbemLocator, &rawLocator);

    if (FAILED(ret) || !rawLocator) {
      log::error(
        "CoCreateInstance for WbemLocator failed, {}",
        formatSystemMessage(ret));

      throw failed();
    }

    m_locator.reset(static_cast<IWbemLocator*>(rawLocator));
  }

  void createService(const std::string& ns)
  {
    IWbemServices* rawService = nullptr;

    const auto res = m_locator->ConnectServer(
      _bstr_t(ns.c_str()),
      nullptr, nullptr, nullptr, 0, nullptr, nullptr,
      &rawService);

    if (FAILED(res) || !rawService) {
      // don't log as error, seems to happen often for some people
      log::debug(
        "locator->ConnectServer() failed for namespace '{}', {}",
        ns, formatSystemMessage(res));

      throw failed();
    }

    m_service.reset(rawService);
  }

  void setSecurity()
  {
    auto ret = CoSetProxyBlanket(
      m_service.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
      RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, 0, EOAC_NONE);

    if (FAILED(ret))
    {
      log::error("CoSetProxyBlanket() failed, {}", formatSystemMessage(ret));
      throw failed();
    }
  }

  COMPtr<IEnumWbemClassObject> getEnumerator(
    const std::string& query)
  {
    IEnumWbemClassObject* rawEnumerator = NULL;

    auto ret = m_service->ExecQuery(
      bstr_t("WQL"),
      bstr_t(query.c_str()),
      WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
      NULL,
      &rawEnumerator);

    if (FAILED(ret) || !rawEnumerator)
    {
      log::error("query '{}' failed, {}", query, formatSystemMessage(ret));
      return {};
    }

    return COMPtr<IEnumWbemClassObject>(rawEnumerator);
  }
};


SecurityProduct::SecurityProduct(
  QUuid guid, QString name, int provider,
  bool active, bool upToDate) :
  m_guid(std::move(guid)), m_name(std::move(name)), m_provider(provider),
  m_active(active), m_upToDate(upToDate)
{
}

const QUuid& SecurityProduct::guid() const
{
  return m_guid;
}

const QString& SecurityProduct::name() const
{
  return m_name;
}

int SecurityProduct::provider() const
{
  return m_provider;
}

bool SecurityProduct::active() const
{
  return m_active;
}

bool SecurityProduct::upToDate() const
{
  return m_upToDate;
}

QString SecurityProduct::toString() const
{
  QString s;

  if (m_name.isEmpty()) {
    s += "(no name)";
  } else {
    s += m_name;
  }

  s += " (" + providerToString() + ")";

  if (!m_active) {
    s += ", inactive";
  }

  if (!m_upToDate) {
    s += ", definitions outdated";
  }

  return s;
}

QString SecurityProduct::providerToString() const
{
  QStringList ps;

  if (m_provider & WSC_SECURITY_PROVIDER_FIREWALL) {
    ps.push_back("firewall");
  }

  if (m_provider & WSC_SECURITY_PROVIDER_AUTOUPDATE_SETTINGS) {
    ps.push_back("autoupdate");
  }

  if (m_provider & WSC_SECURITY_PROVIDER_ANTIVIRUS) {
    ps.push_back("antivirus");
  }

  if (m_provider & WSC_SECURITY_PROVIDER_ANTISPYWARE) {
    ps.push_back("antispyware");
  }

  if (m_provider & WSC_SECURITY_PROVIDER_INTERNET_SETTINGS) {
    ps.push_back("settings");
  }

  if (m_provider & WSC_SECURITY_PROVIDER_USER_ACCOUNT_CONTROL) {
    ps.push_back("uac");
  }

  if (m_provider & WSC_SECURITY_PROVIDER_SERVICE) {
    ps.push_back("service");
  }

  if (ps.empty()) {
    return "doesn't provide anything";
  }

  return ps.join("|");
}


std::optional<SecurityProduct> handleProduct(IWbemClassObject* o)
{
  VARIANT prop;


  // guid
  auto ret = o->Get(L"instanceGuid", 0, &prop, 0, 0);
  if (FAILED(ret)) {
    log::error("failed to get instanceGuid, {}", formatSystemMessage(ret));
    return {};
  }

  if (prop.vt != VT_BSTR) {
    log::error("instanceGuid is a {}, not a bstr", prop.vt);
    return {};
  }

  const QUuid guid(QString::fromWCharArray(prop.bstrVal));
  VariantClear(&prop);


  // display name
  QString displayName;
  ret = o->Get(L"displayName", 0, &prop, 0, 0);

  if (FAILED(ret)) {
    log::error("failed to get displayName, {}", formatSystemMessage(ret));
  } else if (prop.vt != VT_BSTR) {
    log::error("displayName is a {}, not a bstr", prop.vt);
  } else {
    displayName = QString::fromWCharArray(prop.bstrVal);
  }

  VariantClear(&prop);


  // product state
  DWORD state = 0;
  ret = o->Get(L"productState", 0, &prop, 0, 0);

  if (FAILED(ret)) {
    log::error("failed to get productState, {}", formatSystemMessage(ret));
  } else {
    if (prop.vt == VT_I4) {
      state = prop.lVal;
    } else if (prop.vt == VT_UI4) {
      state = prop.ulVal;
    } else if (prop.vt == VT_UI1) {
      state = prop.bVal;
    } else if (prop.vt == VT_NULL) {
      log::warn("productState is null");
    } else {
      log::error("productState is a {}, not a VT_I4 or a VT_UI4", prop.vt);
    }
  }

  VariantClear(&prop);


  const auto provider = static_cast<int>((state >> 16) & 0xff);
  const auto scanner = (state >> 8) & 0xff;
  const auto definitions = state & 0xff;

  const bool active = ((scanner & 0x10) != 0);
  const bool upToDate = (definitions == 0);

  return SecurityProduct(guid, displayName, provider, active, upToDate);
}

std::vector<SecurityProduct> getSecurityProductsFromWMI()
{
  // some products may be present in multiple queries, such as a product marked
  // as both antivirus and antispyware, but they'll have the same GUID, so use
  // that to avoid duplicating entries
  std::map<QUuid, SecurityProduct> map;

  auto f = [&](auto* o) {
    if (auto p=handleProduct(o)) {
      map.emplace(p->guid(), std::move(*p));
    }
  };

  {
    WMI wmi("root\\SecurityCenter2");
    wmi.query("select * from AntivirusProduct", f);
    wmi.query("select * from FirewallProduct", f);
    wmi.query("select * from AntiSpywareProduct", f);
  }

  {
    WMI wmi("root\\SecurityCenter");
    wmi.query("select * from AntivirusProduct", f);
    wmi.query("select * from FirewallProduct", f);
    wmi.query("select * from AntiSpywareProduct", f);
  }

  std::vector<SecurityProduct> v;

  for (auto&& p : map) {
    v.push_back(p.second);
  }

  return v;
}

std::optional<SecurityProduct> getWindowsFirewall()
{
  HRESULT hr = 0;

  COMPtr<INetFwPolicy2> policy;

  {
    void* rawPolicy = nullptr;

    hr = CoCreateInstance(
      __uuidof(NetFwPolicy2), nullptr, CLSCTX_INPROC_SERVER,
      __uuidof(INetFwPolicy2), &rawPolicy);

    if (FAILED(hr) || !rawPolicy) {
      log::error(
        "CoCreateInstance for NetFwPolicy2 failed, {}",
        formatSystemMessage(hr));

      return {};
    }

    policy.reset(static_cast<INetFwPolicy2*>(rawPolicy));
  }

  VARIANT_BOOL enabledVariant;

  if (policy) {
    hr = policy->get_FirewallEnabled(NET_FW_PROFILE2_PUBLIC, &enabledVariant);
    if (FAILED(hr))
    {
      // EPT_S_NOT_REGISTERED is "There are no more endpoints available from the
      // endpoint mapper", which seems to happen sometimes on Windows 7 when the
      // firewall has been disabled, so treat it as such and don't log it
      //
      // however the user reported the error was actually 0x800706d9, not just
      // 0x6d9 (1753, what EPT_S_NOT_REGISTERED is defined to), so this is
      // testing for both because it's not clear which it is and nobody can
      // reproduce it
      if (hr != EPT_S_NOT_REGISTERED && hr != 0x800706d9) {
        // don't log as error
        log::debug("get_FirewallEnabled failed, {}", formatSystemMessage(hr));
      }

      return {};
    }
  }

  const auto enabled = (enabledVariant != VARIANT_FALSE);
  if (!enabled) {
    return {};
  }

  return SecurityProduct(
    {}, "Windows Firewall", WSC_SECURITY_PROVIDER_FIREWALL, true, true);
}


std::vector<SecurityProduct> getSecurityProducts()
{
  std::vector<SecurityProduct> v;

  {
    auto fromWMI = getSecurityProductsFromWMI();
    v.insert(
      v.end(),
      std::make_move_iterator(fromWMI.begin()),
      std::make_move_iterator(fromWMI.end()));
  }

  if (auto p=getWindowsFirewall()) {
    v.push_back(std::move(*p));
  }

  return v;
}


class failed
{
public:
  failed(DWORD e, QString what)
    : m_what(what + ", " + QString::fromStdWString(formatSystemMessage(e)))
  {
  }

  QString what() const
  {
    return m_what;
  }

private:
  QString m_what;
};


MallocPtr<SECURITY_DESCRIPTOR> getSecurityDescriptor(const QString& path)
{
  const auto wpath = path.toStdWString();
  BOOL ret = FALSE;

  DWORD length = 0;
  ret = ::GetFileSecurityW(
    wpath.c_str(), DACL_SECURITY_INFORMATION|OWNER_SECURITY_INFORMATION,
    nullptr, 0, &length);

  if (!ret || length == 0) {
    const auto e = GetLastError();

    if (e != ERROR_INSUFFICIENT_BUFFER) {
      if (e == ERROR_ACCESS_DENIED) {
        // if this fails, the user doesn't even have permissions to get the
        // security descriptor, which probably means they're not the owner and
        // their effective access is none
        throw failed(e, "cannot get security descriptor");
      } else {
        // other error
        throw failed(e, "GetFileSecurity() for length failed");
      }
    }
  }

  MallocPtr<SECURITY_DESCRIPTOR> sd(
    static_cast<SECURITY_DESCRIPTOR*>(std::malloc(length)));

  std::memset(sd.get(), 0, length);

  ret = ::GetFileSecurityW(
    wpath.c_str(), DACL_SECURITY_INFORMATION|OWNER_SECURITY_INFORMATION,
    sd.get(), length, &length);

  if (!ret) {
    const auto e = GetLastError();
    throw failed(e, "GetFileSecurity()");
  }

  return sd;
}

PACL getDacl(SECURITY_DESCRIPTOR* sd)
{
  BOOL present = FALSE;
  BOOL daclDefaulted = FALSE;
  PACL acl = nullptr;

  BOOL ret = ::GetSecurityDescriptorDacl(sd, &present, &acl, &daclDefaulted);

  if (!ret) {
    const auto e = GetLastError();
    throw failed(e, "GetSecurityDescriptorDacl()");
  }

  if (!present) {
    return nullptr;
  }

  return acl;
}

PSID getFileOwner(SECURITY_DESCRIPTOR* sd)
{
  BOOL ownerDefaulted = FALSE;
  PSID owner;

  BOOL ret = ::GetSecurityDescriptorOwner(sd, &owner, &ownerDefaulted);

  if (!ret) {
    const auto e = GetLastError();
    throw failed(e, "GetSecurityDescriptionOwner()");
  }

  return owner;
}

MallocPtr<void> getCurrentUser()
{
  HANDLE hnd = ::GetCurrentProcess();
  HANDLE rawToken = 0;

  BOOL ret = ::OpenProcessToken(hnd, TOKEN_QUERY, &rawToken);
  if (!ret) {
    const auto e = GetLastError();
    throw(e, "OpenProcessToken()");
  }

  HandlePtr token(rawToken);

  DWORD retsize = 0;
  ret = ::GetTokenInformation(token.get(), TokenUser, 0, 0, &retsize);

  if (!ret) {
    const auto e = GetLastError();
    if (e != ERROR_INSUFFICIENT_BUFFER) {
      throw failed(e, "GetTokenInformation() for length");
    }
  }

  MallocPtr<void> tokenBuffer(std::malloc(retsize));
  ret = ::GetTokenInformation(
    token.get(), TokenUser, tokenBuffer.get(), retsize, &retsize);

  if (!ret) {
    const auto e = GetLastError();
    throw failed(e, "GetTokenInformation()");
  }

  PSID tokenSid = ((PTOKEN_USER)(tokenBuffer.get()))->User.Sid;
  DWORD sidLen = ::GetLengthSid(tokenSid);
  MallocPtr<void> currentUserSID((SID*)(malloc(sidLen)));

  ret = ::CopySid(sidLen, currentUserSID.get(), tokenSid);

  if (!ret) {
    const auto e = GetLastError();
    throw failed(e, "CopySid()");
  }

  return currentUserSID;
}

ACCESS_MASK getEffectiveRights(ACL* dacl, PSID sid)
{
  TRUSTEEW trustee = {};
  BuildTrusteeWithSid(&trustee, sid);

  ACCESS_MASK access = 0;
  DWORD ret = ::GetEffectiveRightsFromAclW(dacl, &trustee, &access);

  if (ret != ERROR_SUCCESS) {
    throw failed(ret, "GetEffectiveRightsFromAclW()");
  }

  return access;
}

QString getUsername(PSID owner)
{
  DWORD nameSize=0, domainSize=0;
  auto use = SidTypeUnknown;

  BOOL ret = LookupAccountSidW(
    nullptr, owner, nullptr, &nameSize, nullptr, &domainSize, &use);

  if (!ret) {
    const auto e = GetLastError();

    if (e != ERROR_INSUFFICIENT_BUFFER) {
      throw failed(e, "LookupAccountSid() for sizes");
    }
  }

  auto wsName = std::make_unique<wchar_t[]>(nameSize);
  auto wsDomain = std::make_unique<wchar_t[]>(domainSize);

  ret = LookupAccountSidW(
    nullptr, owner, wsName.get(), &nameSize, wsDomain.get(), &domainSize, &use);

  if (!ret) {
    const auto e = GetLastError();
    throw failed(e, "LookupAccountSid()");
  }

  const QString name = QString::fromWCharArray(wsName.get(), nameSize);
  const QString domain = QString::fromWCharArray(wsDomain.get(), domainSize);

  if (!name.isEmpty() && !domain.isEmpty()) {
    return domain + "\\" + name;
  } else {
    // either or both are empty
    return name + domain;
  }
}

FileRights makeFileRights(ACCESS_MASK m)
{
  FileRights fr;

  if (m & FILE_GENERIC_READ) {
    fr.list.push_back("file_generic_read");
  } else {
    if (m & READ_CONTROL) {
      fr.list.push_back("read_ctrl");
    }

    if (m & FILE_READ_DATA) {
      fr.list.push_back("read_data");
    }

    if (m & FILE_READ_ATTRIBUTES) {
      fr.list.push_back("read_atts");
    }

    if (m & FILE_READ_EA) {
      fr.list.push_back("read_ex_atts");
    }

    if (m & SYNCHRONIZE) {
      fr.list.push_back("sync");
    }
  }

  if (m & FILE_GENERIC_WRITE) {
    fr.list.push_back("file_generic_write");
  } else {
    // READ_CONTROL handled above

    if (m & FILE_WRITE_DATA) {
      fr.list.push_back("write_data");
    }

    if (m & FILE_WRITE_ATTRIBUTES) {
      fr.list.push_back("write_atts");
    }

    if (m & FILE_WRITE_EA) {
      fr.list.push_back("write_ex_atts");
    }

    if (m & FILE_APPEND_DATA) {
      fr.list.push_back("append_data");
    }

    // SYNCHRONIZE handled above
  }

  if (m & FILE_GENERIC_EXECUTE) {
    fr.list.push_back("file_generic_execute");
    fr.hasExecute = true;
  } else {
    // READ_CONTROL handled above
    // FILE_READ_ATTRIBUTES handled above

    if (m & FILE_EXECUTE) {
      fr.list.push_back("execute");
      fr.hasExecute = true;
    }

    // SYNCHRONIZE handled above
  }

  if (m & DELETE) {
    fr.list.push_back("delete");
  }

  if (m & WRITE_DAC) {
    fr.list.push_back("write_dac");
  }

  if (m & WRITE_OWNER) {
    fr.list.push_back("write_owner");
  }

  if (m & GENERIC_ALL) {
    fr.list.push_back("generic_all");
  }

  if (m & GENERIC_WRITE) {
    fr.list.push_back("generic_write");
  }

  if (m & GENERIC_READ) {
    fr.list.push_back("generic_read");
  }

  // 0x001f01ff
  const auto normalRights =
    STANDARD_RIGHTS_ALL |
    FILE_GENERIC_READ | FILE_GENERIC_WRITE | FILE_GENERIC_EXECUTE |
    FILE_DELETE_CHILD;

  if (m == normalRights) {
    fr.normalRights = true;
  }

  return fr;
}

FileSecurity getFileSecurity(const QString& path)
{
  FileSecurity fs;

  try
  {
    auto sd = getSecurityDescriptor(path);
    auto dacl = getDacl(sd.get());
    auto currentUser = getCurrentUser();
    auto owner = getFileOwner(sd.get());
    auto access = getEffectiveRights(dacl, currentUser.get());

    fs.rights = makeFileRights(access);

    if (EqualSid(owner, currentUser.get())) {
      fs.owner = "(this user)";
    } else {
      fs.owner = getUsername(owner);
    }
  }
  catch(failed& f)
  {
    fs.error = f.what();
  }

  return fs;
}
} // namespace
