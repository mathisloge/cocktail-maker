"""Common step definitions shared across all features."""

from datetime import timedelta
from quite import Probe, RemoteObject, query
from pytest_bdd import given, when, parsers


def wait_for_page_change(cocktail_maker_page: RemoteObject):
    """Wait for a page change so that the new page is definitely correctly visible"""
    busy = cocktail_maker_page.property("busy")
    assert busy.wait_for_value(True, timeout=timedelta(seconds=2))
    assert not busy.wait_for_value(False, timeout=timedelta(seconds=2))


@given("the application is running")
def cocktail_maker_app_running(cocktail_maker_app: Probe):
    """Verify application is connected."""
    cocktail_maker_app.wait_for_connected()


@when(parsers.parse('I choose the "{recipe}" recipe'))
def select_recipe(
    cocktail_maker_app: Probe, cocktail_maker_page: RemoteObject, recipe: str
):
    recipe_button = cocktail_maker_app.try_find_object(
        object_query=query().property("objectName", f"recipe_{recipe}"),
        timeout=timedelta(seconds=1),
    )
    recipe_button.mouse_action()
    wait_for_page_change(cocktail_maker_page)


@when(parsers.parse("I boost the recipe with 0%"))
def boost_recipe_and_mix(cocktail_maker_app: Probe, cocktail_maker_page: RemoteObject):
    cocktail_maker_app.find_object(
        query()
        .property("objectName", "mixButton")
        .parent(query().property("objectName", "recipeDetailPage"))
    ).mouse_action()
    wait_for_page_change(cocktail_maker_page)


@when(parsers.parse('I choose the glass "{glassName}"'))
def select_glass_size_and_continue(
    cocktail_maker_app: Probe, cocktail_maker_page: RemoteObject, glassName: str
):
    page_query = query().property("objectName", "glassSelectionPage")
    glass_btn = cocktail_maker_app.find_object(
        object_query=query().property("objectName", f"glass_{glassName}"),
    )
    glass_btn.mouse_action()

    assert (
        cocktail_maker_app.find_object(page_query).property("_selectedGlassId").value()
        == "g3"
    )

    cocktail_maker_app.find_object(
        object_query=query().property("objectName", "mixButton").parent(page_query)
    ).mouse_action()

    wait_for_page_change(cocktail_maker_page)
