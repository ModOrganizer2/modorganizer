#include "envshell.h"
#include "env.h"
#include <log.h>
#include <utility.h>
#include <windowsx.h>

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
  WndProcFilter(QMainWindow* mw, IContextMenu* cm)
    : m_mw(mw), m_cm(cm), m_cm2(nullptr), m_cm3(nullptr)
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

  ~WndProcFilter()
  {
    if (auto* sb=m_mw->statusBar()) {
      sb->clearMessage();
    }
  }

  bool nativeEventFilter(const QByteArray& type, void* m, long* lresultOut) override
  {
    MSG* msg = (MSG*)m;

    if (msg->message == WM_MENUSELECT) {
      HANDLE_WM_MENUSELECT(msg->hwnd, msg->wParam, msg->lParam, onMenuSelect);
      return true;
    }

    if (m_cm3) {
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
  QMainWindow* m_mw;
  IContextMenu* m_cm;
  COMPtr<IContextMenu2> m_cm2;
  COMPtr<IContextMenu3> m_cm3;

  // adapted from
  // https://devblogs.microsoft.com/oldnewthing/20040928-00/?p=37723
  //
  void onMenuSelect(
    HWND hwnd, HMENU hmenu, int item, HMENU hmenuPopup, UINT flags)
  {
    if (m_cm && item >= QCM_FIRST && item <= QCM_LAST) {
      WCHAR szBuf[MAX_PATH];

      const auto r = IContextMenu_GetCommandString(
        m_cm, item - QCM_FIRST, GCS_HELPTEXTW, NULL, szBuf, MAX_PATH);

      if (FAILED(r)) {
        lstrcpynW(szBuf, L"No help available.", MAX_PATH);
      }

      if (m_mw) {
        if (auto* sb=m_mw->statusBar()) {
          sb->showMessage(QString::fromWCharArray(szBuf));
        }
      }
    }
  }

  // adapted from
  // https://devblogs.microsoft.com/oldnewthing/20040928-00/?p=37723
  //
  HRESULT IContextMenu_GetCommandString(
    IContextMenu *pcm, UINT_PTR idCmd, UINT uFlags,
    UINT *pwReserved, LPWSTR pszName, UINT cchMax)
  {
    // Callers are expected to be using Unicode.
    if (!(uFlags & GCS_UNICODE)) {
      return E_INVALIDARG;
    }

    // Some context menu handlers have off-by-one bugs and will
    // overflow the output buffer. Let’s artificially reduce the
    // buffer size so a one-character overflow won’t corrupt memory.
    if (cchMax <= 1) {
      return E_FAIL;
    }

    cchMax--;

    // First try the Unicode message.  Preset the output buffer
    // with a known value because some handlers return S_OK without
    // doing anything.
    pszName[0] = L'\0';

    HRESULT hr = pcm->GetCommandString(
      idCmd, uFlags, pwReserved, (LPSTR)pszName, cchMax);

    if (SUCCEEDED(hr) && pszName[0] == L'\0') {
      // Rats, a buggy IContextMenu handler that returned success
      // even though it failed.
      hr = E_NOTIMPL;
    }

    if (FAILED(hr)) {
      // try again with ANSI – pad the buffer with one extra character
      // to compensate for context menu handlers that overflow by
      // one character.
      LPSTR pszAnsi = (LPSTR)LocalAlloc(
        LMEM_FIXED, (cchMax + 1) * sizeof(CHAR));

      if (pszAnsi) {
        pszAnsi[0] = '\0';

        hr = pcm->GetCommandString(
          idCmd, uFlags & ~GCS_UNICODE, pwReserved, pszAnsi, cchMax);

        if (SUCCEEDED(hr) && pszAnsi[0] == '\0') {
          // Rats, a buggy IContextMenu handler that returned success
          // even though it failed.
          hr = E_NOTIMPL;
        }

        if (SUCCEEDED(hr)) {
          if (MultiByteToWideChar(CP_ACP, 0, pszAnsi, -1, pszName, cchMax) == 0) {
            hr = E_FAIL;
          }
        }

        LocalFree(pszAnsi);

      } else {
        hr = E_OUTOFMEMORY;
      }
    }

    return hr;
  }
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

int runMenu(QMainWindow* mw, IContextMenu* cm, HMENU menu, const QPoint& p)
{
  const auto hwnd = (HWND)mw->winId();

  auto filter = std::make_unique<WndProcFilter>(mw, cm);
  QCoreApplication::instance()->installNativeEventFilter(filter.get());

  return TrackPopupMenuEx(menu, TPM_RETURNCMD, p.x(), p.y(), hwnd, nullptr);
}

void invoke(QMainWindow* mw, const QPoint& p, int cmd, IContextMenu* cm)
{
  const auto hwnd = (HWND)mw->winId();

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

QMainWindow* getMainWindow(QWidget* w)
{
  QWidget* p = w;

  while (p) {
    if (auto* mw=dynamic_cast<QMainWindow*>(p)) {
      return mw;
    }

    p = p->parentWidget();
  }

  return nullptr;
}

void showShellMenu(QWidget* parent, const QFileInfo& file, const QPoint& pos)
{
  const auto path = QDir::toNativeSeparators(file.absoluteFilePath());

  try
  {
    auto* mw = getMainWindow(parent);
    auto idl = getIDL(path.toStdWString().c_str());
    auto [sf, childIdl] = getShellFolder(idl.get());
    auto cm = getContextMenu(sf.get(), childIdl);
    auto hmenu = createMenu(cm.get());

    const int cmd = runMenu(mw, cm.get(), hmenu.get(), pos);
    if (cmd <= 0) {
      return;
    }

    invoke(mw, pos, cmd - QCM_FIRST, cm.get());
  }
  catch(MenuFailed& e)
  {
    log::error("can't create shell menu for '{}': {}", path, e.what());
  }
}

} // namespace
