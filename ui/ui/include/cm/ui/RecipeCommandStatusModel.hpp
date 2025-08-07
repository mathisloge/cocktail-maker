#pragma once
#include <QAbstractItemModel>
#include <QtQmlIntegration>
#include "cm/commands/command_id.hpp"

namespace cm::ui
{
class RecipeCommandStatusModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Will be provided by the RecipeExecutorAdapter")
  public:
    enum class Roles
    {
        name = Qt::UserRole + 1,
        status
    };
    enum class CommandStatus
    {
        NotStarted, // NOLINT
        Started,    // NOLINT
        Finished    // NOLINT
    };
    Q_ENUM(CommandStatus)

    RecipeCommandStatusModel();
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    void reset();
    void register_command(CommandId id, QString name);
    void command_started(CommandId id);
    void command_finished(CommandId id);

  private:
    struct Data
    {
        QString name;
        CommandStatus status;
    };
    std::map<CommandId, Data> commands_;
};
} // namespace cm::ui
