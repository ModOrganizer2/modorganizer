#include "envsecurity.h"
#include "env.h"
#include <utility.h>

#include <Wbemidl.h>
#include <wscapi.h>
#include <comdef.h>
#include <netfw.h>
#pragma comment(lib, "Wbemuuid.lib")

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
          qCritical()
            << "enumerator->next() failed, " << formatSystemMessageQ(ret);
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
      qCritical()
        << "CoCreateInstance for WbemLocator failed, "
        << formatSystemMessageQ(ret);

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
      qCritical()
        << "locator->ConnectServer() failed for namespace "
        << "'" << QString::fromStdString(ns) << "', "
        << formatSystemMessageQ(res);

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
      qCritical()
        << "CoSetProxyBlanket() failed, " << formatSystemMessageQ(ret);

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
      qCritical()
        << "query '" << QString::fromStdString(query) << "' failed, "
        << formatSystemMessageQ(ret);

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

  s += m_name + " (" + providerToString() + ")";

  if (!m_active) {
    s += ", inactive";
  }

  if (!m_upToDate) {
    s += ", definitions outdated";
  }

  if (!m_guid.isNull()) {
    s += ", " + m_guid.toString(QUuid::QUuid::WithoutBraces);
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
    return "doesn't provider anything";
  }

  return ps.join("|");
}


std::vector<SecurityProduct> getSecurityProductsFromWMI()
{
  // some products may be present in multiple queries, such as a product marked
  // as both antivirus and antispyware, but they'll have the same GUID, so use
  // that to avoid duplicating entries
  std::map<QUuid, SecurityProduct> map;

  auto handleProduct = [&](auto* o) {
    VARIANT prop;

    // display name
    auto ret = o->Get(L"displayName", 0, &prop, 0, 0);
    if (FAILED(ret)) {
      qCritical()
        << "failed to get displayName, "
        << formatSystemMessageQ(ret);

      return;
    }

    if (prop.vt != VT_BSTR) {
      qCritical() << "displayName is a " << prop.vt << ", not a bstr";
      return;
    }

    const std::wstring name = prop.bstrVal;
    VariantClear(&prop);

    // product state
    ret = o->Get(L"productState", 0, &prop, 0, 0);
    if (FAILED(ret)) {
      qCritical()
        << "failed to get productState, "
        << formatSystemMessageQ(ret);

      return;
    }

    if (prop.vt != VT_UI4 && prop.vt != VT_I4) {
      qCritical() << "productState is a " << prop.vt << ", is not a VT_UI4";
      return;
    }

    DWORD state = 0;
    if (prop.vt == VT_I4) {
      state = prop.lVal;
    } else {
      state = prop.ulVal;
    }

    VariantClear(&prop);

    // guid
    ret = o->Get(L"instanceGuid", 0, &prop, 0, 0);
    if (FAILED(ret)) {
      qCritical()
        << "failed to get instanceGuid, "
        << formatSystemMessageQ(ret);

      return;
    }

    if (prop.vt != VT_BSTR) {
      qCritical() << "instanceGuid is a " << prop.vt << ", is not a bstr";
      return;
    }

    const QUuid guid(QString::fromWCharArray(prop.bstrVal));
    VariantClear(&prop);

    const auto provider = static_cast<int>((state >> 16) & 0xff);
    const auto scanner = (state >> 8) & 0xff;
    const auto definitions = state & 0xff;

    const bool active = ((scanner & 0x10) != 0);
    const bool upToDate = (definitions == 0);

    map.insert({
      guid,
      {guid, QString::fromStdWString(name), provider, active, upToDate}});
  };

  {
    WMI wmi("root\\SecurityCenter2");
    wmi.query("select * from AntivirusProduct", handleProduct);
    wmi.query("select * from FirewallProduct", handleProduct);
    wmi.query("select * from AntiSpywareProduct", handleProduct);
  }

  {
    WMI wmi("root\\SecurityCenter");
    wmi.query("select * from AntivirusProduct", handleProduct);
    wmi.query("select * from FirewallProduct", handleProduct);
    wmi.query("select * from AntiSpywareProduct", handleProduct);
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
      qCritical()
        << "CoCreateInstance for NetFwPolicy2 failed, "
        << formatSystemMessageQ(hr);

      return {};
    }

    policy.reset(static_cast<INetFwPolicy2*>(rawPolicy));
  }

  VARIANT_BOOL enabledVariant;

  if (policy) {
    hr = policy->get_FirewallEnabled(NET_FW_PROFILE2_PUBLIC, &enabledVariant);
    if (FAILED(hr))
    {
      qCritical()
        << "get_FirewallEnabled failed, "
        << formatSystemMessageQ(hr);

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

} // namespace
