"""
Create planning formalism structures. 

This example demonstrates how to create planning formalism structures.
Bindings are currently not exhaustively added.

Example usage (run from the repository root):

    python3 python/examples/formalism/planning/builder.py \
        -d data/gripper/domain.pddl \
        -p data/gripper/test_problem.pddl

Author: Dominik Drexler (dominik.drexler@liu.se)
"""

import argparse

from pathlib import Path

from pytyr.formalism.planning import ParserOptions, Parser, ObjectBuilder, FluentPredicateBuilder, FluentGroundAtomBuilder


def main():
    arg_parser = argparse.ArgumentParser(description="Create planning formalism structures.")
    arg_parser.add_argument("-d", "--domain-filepath", type=Path, required=True, help="Path to a PDDL domain file.")
    arg_parser.add_argument("-p", "--task-filepath", type=Path, required=True, help="Path to PDDL task file.")
    args = arg_parser.parse_args()

    domain_filepath : Path = args.domain_filepath
    task_filepath : Path = args.task_filepath

    parser_options = ParserOptions()
    parser = Parser(domain_filepath, parser_options)
    
    ### 1. Domain: a domain an be shared among arbitrarily many downstream tasks.

    planning_task = parser.parse_task(task_filepath, parser_options)

    repository = planning_task.get_repository()

    object_builder = ObjectBuilder()
    fluent_predicate_builder = FluentPredicateBuilder()
    fluent_ground_atom_builder = FluentGroundAtomBuilder()
    
    # Re-create existing atom (at ball1 rooma)
    object_builder.name = "ball1"
    ball1, inserted = repository.get_or_create_object(object_builder)
    assert not inserted
    object_builder.name = "rooma"
    rooma, inserted = repository.get_or_create_object(object_builder)
    assert not inserted 

    fluent_predicate_builder.name = "at"
    fluent_predicate_builder.arity = 2
    at, inserted = repository.get_or_create_fluent_predicate(fluent_predicate_builder)
    assert not inserted

    row, inserted = repository.get_or_create_fluent_predicate_row(at, [ball1, rooma])
    assert not inserted

    fluent_ground_atom_builder.predicate = at 
    fluent_ground_atom_builder.row = row
    at_ball1_rooma, inserted = repository.get_or_create_fluent_ground_atom(fluent_ground_atom_builder)
    assert not inserted

    # Create non existing atom (at ball1 roomb)
    object_builder.name = "roomb"
    roomb, inserted = repository.get_or_create_object(object_builder)
    assert not inserted 

    row, inserted = repository.get_or_create_fluent_predicate_row(at, [ball1, roomb])
    assert inserted 

    fluent_ground_atom_builder.predicate = at 
    fluent_ground_atom_builder.row = row
    at_ball1_roomb = repository.get_or_create_fluent_ground_atom(fluent_ground_atom_builder)
    assert inserted