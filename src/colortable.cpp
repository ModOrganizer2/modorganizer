#include "colortable.h"
#include "modconflicticondelegate.h"
#include "modflagicondelegate.h"
#include "settings.h"

class ColorItem;
ColorItem* colorItemForRow(QTableWidget* table, int row);

void paintBackground(QTableWidget* table, QPainter* p,
                     const QStyleOptionViewItem& option, const QModelIndex& index);

// delegate for the sample text column; paints the background color
//
class ColoredBackgroundDelegate : public QStyledItemDelegate
{
public:
  ColoredBackgroundDelegate(QTableWidget* table) : m_table(table) {}

  void paint(QPainter* p, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override
  {
    paintBackground(m_table, p, option, index);

    QStyleOptionViewItem itemOption(option);
    initStyleOption(&itemOption, index);

    // paint the default stuff like text, but override the state to avoid
    // destroying the background for selected items, etc.
    itemOption.state = QStyle::State_Enabled;

    QStyledItemDelegate::paint(p, itemOption, index);
  }

private:
  QTableWidget* m_table;
};

// delegate for the icons column; paints the background and icons
//
class FakeModFlagIconDelegate : public ModFlagIconDelegate
{
public:
  explicit FakeModFlagIconDelegate(QTableWidget* table) : m_table(table) {}

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override
  {
    paintBackground(m_table, painter, option, index);
    ModFlagIconDelegate::paintIcons(painter, option, index, getIcons(index));
  }

protected:
  QList<QString> getIcons(const QModelIndex& index) const override
  {
    const auto flags = {ModInfo::FLAG_BACKUP, ModInfo::FLAG_NOTENDORSED,
                        ModInfo::FLAG_NOTES, ModInfo::FLAG_ALTERNATE_GAME};

    return getIconsForFlags(flags, false);
  }

  size_t getNumIcons(const QModelIndex& index) const override
  {
    return getIcons(index).size();
  }

private:
  QTableWidget* m_table;
};

// delegate for the icons column; paints the background and icons
//
class FakeModConflictIconDelegate : public ModConflictIconDelegate
{
public:
  explicit FakeModConflictIconDelegate(QTableWidget* table) : m_table(table) {}

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override
  {
    paintBackground(m_table, painter, option, index);
    ModFlagIconDelegate::paintIcons(painter, option, index, getIcons(index));
  }

protected:
  QList<QString> getIcons(const QModelIndex& index) const override
  {
    const auto flags = {ModInfo::FLAG_CONFLICT_MIXED,
                        ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITE,
                        ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITTEN,
                        ModInfo::FLAG_ARCHIVE_CONFLICT_MIXED};

    return getIconsForFlags(flags, false);
  }

  size_t getNumIcons(const QModelIndex& index) const override
  {
    return getIcons(index).size();
  }

private:
  QTableWidget* m_table;
};

// item used in the first column of the table
//
class ColorItem : public QTableWidgetItem
{
public:
  ColorItem(QString caption, QColor defaultColor, std::function<QColor()> get,
            std::function<void(const QColor&)> commit)
      : m_caption(std::move(caption)), m_default(defaultColor), m_get(get),
        m_commit(commit)
  {
    setText(m_caption);
    set(get());
  }

  // color caption
  //
  const QString& caption() const { return m_caption; }

  // the current color
  //
  QColor get() const { return m_temp; }

  // sets the current color, commit() must be called to save it
  //
  bool set(const QColor& c)
  {
    if (m_temp != c) {
      m_temp = c;
      return true;
    }

    return false;
  }

  // resets the current color, commit() must be called to save it
  //
  bool reset() { return set(m_default); }

  // saves the current color
  //
  void commit() { m_commit(m_temp); }

private:
  const QString m_caption;
  const QColor m_default;
  std::function<QColor()> m_get;
  std::function<void(const QColor&)> m_commit;
  QColor m_temp;
};

ColorItem* colorItemForRow(QTableWidget* table, int row)
{
  return dynamic_cast<ColorItem*>(table->item(row, 0));
}

template <class F>
void forEachColorItem(QTableWidget* table, F&& f)
{
  const auto rowCount = table->rowCount();

  for (int i = 0; i < rowCount; ++i) {
    if (auto* ci = colorItemForRow(table, i)) {
      f(ci);
    }
  }
}

void paintBackground(QTableWidget* table, QPainter* p,
                     const QStyleOptionViewItem& option, const QModelIndex& index)
{
  if (auto* ci = colorItemForRow(table, index.row())) {
    p->save();
    p->fillRect(option.rect, ci->get());
    p->restore();
  }
}

ColorTable::ColorTable(QWidget* parent) : QTableWidget(parent), m_settings(nullptr)
{
  setColumnCount(4);
  setHorizontalHeaderLabels({"", "", "", ""});

  setItemDelegateForColumn(1, new ColoredBackgroundDelegate(this));
  setItemDelegateForColumn(2, new FakeModConflictIconDelegate(this));
  setItemDelegateForColumn(3, new FakeModFlagIconDelegate(this));

  connect(this, &QTableWidget::cellActivated, [&] {
    onColorActivated();
  });
}

void ColorTable::load(Settings& s)
{
  m_settings = &s;

  addColor(
      QObject::tr("Is overwritten (loose files)"), QColor(0, 255, 0, 64),
      [this] {
        return m_settings->colors().modlistOverwrittenLoose();
      },
      [this](auto&& v) {
        m_settings->colors().setModlistOverwrittenLoose(v);
      });

  addColor(
      QObject::tr("Is overwriting (loose files)"), QColor(255, 0, 0, 64),
      [this] {
        return m_settings->colors().modlistOverwritingLoose();
      },
      [this](auto&& v) {
        m_settings->colors().setModlistOverwritingLoose(v);
      });

  addColor(
      QObject::tr("Is overwritten (archives)"), QColor(0, 255, 255, 64),
      [this] {
        return m_settings->colors().modlistOverwrittenArchive();
      },
      [this](auto&& v) {
        m_settings->colors().setModlistOverwrittenArchive(v);
      });

  addColor(
      QObject::tr("Is overwriting (archives)"), QColor(255, 0, 255, 64),
      [this] {
        return m_settings->colors().modlistOverwritingArchive();
      },
      [this](auto&& v) {
        m_settings->colors().setModlistOverwritingArchive(v);
      });

  addColor(
      QObject::tr("Mod contains selected file"), QColor(0, 0, 255, 64),
      [this] {
        return m_settings->colors().modlistContainsFile();
      },
      [this](auto&& v) {
        m_settings->colors().setModlistContainsFile(v);
      });

  addColor(
      QObject::tr("Plugin is contained in selected mod"), QColor(0, 0, 255, 64),
      [this] {
        return m_settings->colors().pluginListContained();
      },
      [this](auto&& v) {
        m_settings->colors().setPluginListContained(v);
      });

  addColor(
      QObject::tr("Plugin is master of selected plugin"), QColor(255, 255, 0, 64),
      [this] {
        return m_settings->colors().pluginListMaster();
      },
      [this](auto&& v) {
        m_settings->colors().setPluginListMaster(v);
      });
}

void ColorTable::resetColors()
{
  bool changed = false;

  forEachColorItem(this, [&](auto* item) {
    if (item->reset()) {
      changed = true;
    }
  });

  if (changed) {
    update();
  }
}

void ColorTable::commitColors()
{
  forEachColorItem(this, [](auto* item) {
    item->commit();
  });
}

void ColorTable::addColor(const QString& text, const QColor& defaultColor,
                          std::function<QColor()> get,
                          std::function<void(const QColor&)> commit)
{
  const auto r = rowCount();
  setRowCount(r + 1);

  setItem(r, 0, new ColorItem(text, defaultColor, get, commit));
  setItem(r, 1, new QTableWidgetItem("Text"));
  setItem(r, 2, new QTableWidgetItem);

  resizeColumnsToContents();
}

void ColorTable::onColorActivated()
{
  const auto rows = selectionModel()->selectedRows();
  if (rows.isEmpty()) {
    return;
  }

  const auto row = rows[0].row();
  auto* ci       = colorItemForRow(this, row);
  if (!ci) {
    return;
  }

  const QColor result = QColorDialog::getColor(
      ci->get(), topLevelWidget(), ci->caption(), QColorDialog::ShowAlphaChannel);

  if (result.isValid()) {
    ci->set(result);
    update(model()->index(row, 1));
  }
}
