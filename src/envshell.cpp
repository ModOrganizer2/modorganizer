#include "envshell.h"
#include "env.h"
#include <log.h>
#include <utility.h>

namespace env
{

using namespace MOBase;

const int QCM_FIRST = 1;
const int QCM_LAST = 0x7ff;

class MenuFailed : public std::runtime_error
{
public:
  MenuFailed(HRESULT r, const std::string& what)
    : runtime_error(fmt::format(
      "{}, {}",
      what, QString::fromStdWString(formatSystemMessage(r)).toStdString()))
  {
  }
};


class WndProcFilter : public QAbstractNativeEventFilter
{
public:
  WndProcFilter(IContextMenu* cm)
    : m_cm2(nullptr), m_cm3(nullptr)
  {
    IContextMenu2* cm2 = nullptr;
    if (SUCCEEDED(cm->QueryInterface(IID_IContextMenu2, (void**)&cm2))) {
      m_cm2.reset(cm2);
    }

    IContextMenu3* cm3 = nullptr;
    if (SUCCEEDED(cm->QueryInterface(IID_IContextMenu3, (void**)&cm3))) {
      m_cm3.reset(cm3);
    }
  }

  bool nativeEventFilter(const QByteArray& type, void* m, long* lresultOut) override
  {
    if (m_cm3) {
      MSG* msg = (MSG*)m;
      LRESULT lresult = 0;

      const auto r = m_cm3->HandleMenuMsg2(
        msg->message, msg->wParam, msg->lParam, &lresult);

      if (SUCCEEDED(r)) {
        if (lresultOut) {
          *lresultOut = lresult;
        }

        return true;
      }
    }

    if (m_cm2) {
      MSG* msg = (MSG*)m;

      const auto r = m_cm2->HandleMenuMsg(
        msg->message, msg->wParam, msg->lParam);

      if (SUCCEEDED(r)) {
        if (lresultOut) {
          *lresultOut = 0;
        }

        return true;
      }
    }

    return false;
  }

private:
  COMPtr<IContextMenu2> m_cm2;
  COMPtr<IContextMenu3> m_cm3;
};



CoTaskMemPtr<LPITEMIDLIST> getIDL(const wchar_t* path)
{
  LPITEMIDLIST pidl;
  SFGAOF sfgao;

  const auto r = SHParseDisplayName(path, nullptr, &pidl, 0, &sfgao);

  if (FAILED(r)) {
    throw MenuFailed(r, "SHParseDisplayName failed");
  }

  return CoTaskMemPtr<LPITEMIDLIST>(pidl);
}

std::pair<COMPtr<IShellFolder>, LPCITEMIDLIST> getShellFolder(LPITEMIDLIST idl)
{
  IShellFolder* psf = nullptr;
  LPCITEMIDLIST pidlChild = nullptr;

  const auto r = SHBindToParent(
    idl, IID_IShellFolder, reinterpret_cast<void**>(&psf), &pidlChild);

  if (FAILED(r)) {
    throw MenuFailed(r, "SHBindToParent failed");
  }

  return {COMPtr<IShellFolder>(psf), pidlChild};
}

COMPtr<IContextMenu> getContextMenu(IShellFolder* psf, LPCITEMIDLIST idl)
{
  IContextMenu* pcm = nullptr;

  const auto r = psf->GetUIObjectOf(
    0, 1, &idl, IID_IContextMenu, nullptr,
    reinterpret_cast<void**>(&pcm));

  if (FAILED(r)) {
    throw MenuFailed(r, "GetUIObjectOf failed");
  }

  return COMPtr<IContextMenu>(pcm);
}

HMenuPtr createMenu(IContextMenu* cm)
{
  HMENU hmenu = CreatePopupMenu();
  if (!hmenu) {
    const auto e = GetLastError();
    throw MenuFailed(e, "CreatePopupMenu failed");
  }

  const auto r = cm->QueryContextMenu(
    hmenu, 0, QCM_FIRST, QCM_LAST, CMF_EXTENDEDVERBS);

  if (FAILED(r)) {
    throw MenuFailed(r, "QueryContextMenu failed");
  }

  return HMenuPtr(hmenu);
}

int runMenu(IContextMenu* cm, HWND hwnd, HMENU menu, const QPoint& p)
{
  auto filter = std::make_unique<WndProcFilter>(cm);
  QCoreApplication::instance()->installNativeEventFilter(filter.get());

  return TrackPopupMenuEx(menu, TPM_RETURNCMD, p.x(), p.y(), hwnd, nullptr);
}

void invoke(HWND hwnd, const QPoint& p, int cmd, IContextMenu* cm)
{
  CMINVOKECOMMANDINFOEX info = {};

  info.cbSize = sizeof(info);
  info.fMask = CMIC_MASK_UNICODE | CMIC_MASK_PTINVOKE;
  info.hwnd = hwnd;
  info.lpVerb = MAKEINTRESOURCEA(cmd);
  info.lpVerbW = MAKEINTRESOURCEW(cmd);
  info.nShow = SW_SHOWNORMAL;
  info.ptInvoke = {p.x(), p.y()};

  // note: this calls the query version because the Qt even loop hasn't run
  // yet and shift is still considered pressed
  const auto m = QApplication::queryKeyboardModifiers();

  if (m & Qt::ShiftModifier) {
    info.fMask |= CMIC_MASK_SHIFT_DOWN;
  }

  if (m & Qt::ControlModifier) {
    info.fMask |= CMIC_MASK_CONTROL_DOWN;
  }

  const auto r = cm->InvokeCommand((CMINVOKECOMMANDINFO*)&info);

  if (FAILED(r)) {
    throw MenuFailed(r, fmt::format("InvokeCommand failed, verb={}", cmd));
  }
}

void showShellMenu(QWidget* parent, const QFileInfo& file, const QPoint& pos)
{
  const auto path = QDir::toNativeSeparators(file.absoluteFilePath());

  try
  {
    auto idl = getIDL(path.toStdWString().c_str());
    auto [sf, childIdl] = getShellFolder(idl.get());
    auto cm = getContextMenu(sf.get(), childIdl);
    auto hmenu = createMenu(cm.get());
    auto hwnd = (HWND)parent->window()->winId();

    const int cmd = runMenu(cm.get(), hwnd, hmenu.get(), pos);
    if (cmd <= 0) {
      return;
    }

    invoke(hwnd, pos, cmd - QCM_FIRST, cm.get());
  }
  catch(MenuFailed& e)
  {
    log::error("can't create shell menu for '{}': {}", path, e.what());
  }
}

} // namespace
