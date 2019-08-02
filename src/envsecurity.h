#include <QUuid>
#include <QString>

namespace env
{

// represents a security product, such as an antivirus or a firewall
//
class SecurityProduct
{
public:
  SecurityProduct(
    QUuid guid, QString name, int provider,
    bool active, bool upToDate);

  // display name of the product
  //
  const QString& name() const;

  // a bunch of _WSC_SECURITY_PROVIDER flags
  //
  int provider() const;

  // whether the product is active
  //
  bool active() const;

  // whether its definitions are up-to-date
  //
  bool upToDate() const;

  // string representation of the above
  //
  QString toString() const;

private:
  QUuid m_guid;
  QString m_name;
  int m_provider;
  bool m_active;
  bool m_upToDate;

  QString providerToString() const;
};


std::vector<SecurityProduct> getSecurityProducts();

} // namespace env
