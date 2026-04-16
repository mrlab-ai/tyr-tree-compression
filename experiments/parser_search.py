#! /usr/bin/env python

from lab.parser import Parser
from lab import tools
from lab.reports import Attribute, geometric_mean, arithmetic_mean

import re


def process_invalid(content, props):
    props["invalid"] = int("invalid" in props)


def process_unsolvable(content, props):
    props["unsolvable"] = int("unsolvable" in props)


def add_search_time_s(content, props):
    if "search_time_ns" in props:
        props["search_time_s"] = props["search_time_ns"] / 1_000_000_000


def out_of_memory(content, props):
    props["out_of_memory"] = int("std_bad_alloc" in props)


def out_of_time(content, props):
    props["out_of_time"] = int(
        props["out_of_memory"] == 0
        and props["coverage"] == 0
        and props["unsolvable"] == 0
    )


def add_total_time_s(content, props):
    if "total_time_ns" in props:
        props["total_time_s"] = props["total_time_ns"] / 1_000_000_000


def add_preprocessing_time_s(content, props):
    if "total_time_ns" in props and "search_time_ns" in props:
        props["preprocessing_time_s"] = (
            props["total_time_ns"] - props["search_time_ns"]
        ) / 1_000_000_000


def add_search_time_ms_per_expanded(context, props):
    if "search_time_ns" in props and "num_expanded" in props:
        if props["num_expanded"] > 0:
            props["search_time_ms_per_expanded"] = (
                props["search_time_ns"] / 1_000_000 / props["num_expanded"]
            )


def make_add_score_peak_memory_usage_bytes(max_memory_bytes: int):
    def add_scores(content, props):
        success = props["coverage"] or props["unsolvable"]

        props["score_peak_memory_usage_bytes"] = tools.compute_log_score(
            success,
            props.get("peak_memory_usage_bytes"),
            lower_bound=2_000_000,
            upper_bound=max_memory_bytes,
        )

    return add_scores


def make_add_score_state_peak_memory_usage_bytes(max_memory_bytes: int):
    def add_scores(content, props):
        success = props["coverage"] or props["unsolvable"]

        props["score_state_peak_memory_usage_bytes"] = tools.compute_log_score(
            success,
            props.get("state_peak_memory_usage_bytes"),
            lower_bound=2_000_000,
            upper_bound=max_memory_bytes,
        )

    return add_scores


def add_memory_mb(content, props):
    if "peak_memory_usage_bytes" in props:
        props["memory_mb"] = props["peak_memory_usage_bytes"] / 1_000_000


def add_coverage(content, props):
    if "length" in props or props.get("unsolvable", 0):
        props["coverage"] = 1
    else:
        props["coverage"] = 0


class SearchParser(Parser):
    """
    Num objects: 4
    [GBFS] Search started.
    [GBFS] Start node h_value: 3
    [GBFS] New best h_value: 2 with num expanded states 3 and num generated states 5 (0 ms)
    [GBFS] New best h_value: 1 with num expanded states 4 and num generated states 7 (0 ms)
    [GBFS] Search ended.
    [Search] Search time: 0 ms (743179 ns)
    [Search] Number of expanded states: 4
    [Search] Number of generated states: 7
    [Search] Number of pruned states: 0
    [GBFS] Plan found.
    [GBFS] Plan cost: 3
    [GBFS] Plan length: 3
    (pick ball2 rooma left)
    (move rooma roomb)
    (drop ball2 roomb left)

    ...

    [Total] Peak memory usage: 513306624 bytes
    [Total] Total time: 4 ms (4424855 ns)

    """

    def __init__(self, max_memory_bytes: int):
        super().__init__()
        prefix = r"\[[^\]]+\] "
        self.add_pattern("cost", prefix + r"Plan cost: (\d+)", type=int)
        self.add_pattern("length", prefix + r"Plan length: (\d+)", type=int)
        self.add_pattern(
            "initial_h_value", prefix + r"Start node h_value: (\d+)", type=int
        )

        self.add_pattern("search_time_ms", prefix + r"Search time: (\d+) ms", type=int)
        self.add_pattern(
            "search_time_ns", prefix + r"Search time: \d+ ms \((\d+) ns\)", type=int
        )
        self.add_pattern(
            "num_expanded", prefix + r"Number of expanded states: (\d+)", type=int
        )
        self.add_pattern(
            "num_generated", prefix + r"Number of generated states: (\d+)", type=int
        )

        self.add_pattern("total_time_ms", prefix + r"Total time: (\d+) ms", type=int)
        self.add_pattern(
            "total_time_ns", prefix + r"Total time: \d+ ms \((\d+) ns\)", type=int
        )
        self.add_pattern(
            "num_fluent_atoms", prefix + r"Number of fluent atoms: (\d+)", type=int
        )
        self.add_pattern(
            "num_derived_atoms", prefix + r"Number of derived atoms: (\d+)", type=int
        )
        self.add_pattern(
            "num_fluent_fterms", prefix + r"Number of fluent fterms: (\d+)", type=int
        )
        self.add_pattern(
            "states_memory_usage_bytes",
            prefix + r"States memory usage: (\d+) bytes",
            type=int,
        )
        self.add_pattern(
            "peak_memory_usage_bytes",
            prefix + r"Peak memory usage: (\d+) bytes",
            type=int,
        )

        self.add_pattern(
            "std_bad_alloc", r".*(std::bad_alloc).*", type=str, file="run.err"
        )

        self.add_pattern("unsolvable", r"(Task is unsolvable!)", type=str)
        self.add_pattern("invalid", r"(Plan invalid)", type=str)

        self.add_pattern("num_objects", r"Num objects: (\d+)", type=int)

        self.add_function(process_invalid)
        self.add_function(process_unsolvable)
        self.add_function(add_search_time_s)
        self.add_function(add_total_time_s)
        self.add_function(add_preprocessing_time_s)
        self.add_function(add_search_time_ms_per_expanded)
        self.add_function(add_memory_mb)
        self.add_function(add_coverage)

        self.add_function(make_add_score_peak_memory_usage_bytes(max_memory_bytes))
        self.add_function(
            make_add_score_state_peak_memory_usage_bytes(max_memory_bytes)
        )

        self.add_function(out_of_memory)
        self.add_function(out_of_time)

    @staticmethod
    def get_attributes():
        return [
            Attribute("coverage", min_wins=False),
            "out_of_memory",
            "out_of_time",
            "cost",
            "length",
            "unsolvable",
            "invalid",
            "initial_h_value",
            Attribute("search_time_s", function=geometric_mean, digits=2),
            "num_expanded",
            "num_generated",
            Attribute("search_time_ms_per_expanded", function=geometric_mean, digits=2),
            Attribute("total_time_s", function=geometric_mean, digits=2),
            Attribute("preprocessing_time_s", function=geometric_mean, digits=2),
            "num_fluent_atoms",
            "num_derived_atoms",
            "num_fluent_fterms",
            Attribute("states_memory_usage_bytes", function=geometric_mean),
            Attribute("peak_memory_usage_bytes", function=geometric_mean),
            Attribute("memory_mb", function=geometric_mean),
            "score_peak_memory_usage_bytes",
            "score_state_peak_memory_usage_bytes",
        ]
