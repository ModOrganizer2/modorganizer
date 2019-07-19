#include <QString>
#include <vector>

namespace env
{

class Metrics
{
public:
  struct Display
  {
    int resX=0, resY=0, dpi=0;
    bool primary=false;
    int refreshRate = 0;
    QString monitor, adapter;

    QString toString() const;
  };

  Metrics();

  const std::vector<Display>& displays() const;

private:
  std::vector<Display> m_displays;
};

} // namespace
