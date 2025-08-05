#pragma once
#include <QObject>
#include <QtQmlIntegration>
#include "cm/recipe.hpp"

namespace cm::ui
{
struct RecipeStepDetail
{
    Q_GADGET
    QML_VALUE_TYPE(recipeStepDetail)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString detail MEMBER detail)
  public:
    QString name;
    QString detail;
};

class RecipeDetail : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created with the RecipeFactory")
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString description READ description CONSTANT)
    Q_PROPERTY(QString imageSource READ image_path CONSTANT)
    Q_PROPERTY(QList<cm::ui::RecipeStepDetail> steps READ steps CONSTANT)
  public:
    explicit RecipeDetail(std::shared_ptr<Recipe> recipe);
    QString name() const;
    QString description() const;
    QString image_path() const;
    const QList<cm::ui::RecipeStepDetail> &steps() const;
    [[nodiscard]] std::shared_ptr<Recipe> recipe() const;

  private:
    std::shared_ptr<Recipe> recipe_;
    QList<RecipeStepDetail> steps_;
};
} // namespace cm::ui
