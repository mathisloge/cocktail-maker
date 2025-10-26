Feature: Create cocktail
    As a user I want to create a cocktail with the listed ingredients.

    Background:
        Given the application is running


    Scenario: Successful creation
        When I choose the "Mojito" recipe
        And I boost the recipe with 0%
        And I choose the glass "Glass3"
