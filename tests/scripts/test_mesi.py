#!/usr/bin/env python3

import argparse
from dataclasses import dataclass
from enum import Enum


class CacheStatus(Enum):
    MODIFIED = "M"
    EXCLUSIVE = "E"
    SHARED = "S"
    INVALID = "I"

    def __str__(self):
        return self.value


@dataclass
class CacheLine:
    set_index: int
    tag: int
    last_used: int
    status: CacheStatus

    def __post_init__(self):
        self.set_index = int(self.set_index)
        self.tag = int(self.tag)
        self.last_used = int(self.last_used)

        self.status = CacheStatus(self.status)


def process_cache(lines):
    cache_content_found = False
    cache_contents = {}

    cache_id = 0
    for line in lines:
        if not cache_content_found and "CACHE CONTENT" in line:
            cache_content_found = True
        elif cache_content_found and "CACHE END" in line:
            cache_content_found = False

        if not cache_content_found:
            continue

        if "CACHE CONTENT" in line:
            continue

        if "Cache " in line:
            cache_id = int(line.split(" ")[1].strip(":"))
            cache_contents[cache_id] = []
        else:
            line = line.strip("\t").strip("\n")
            line = line.lstrip("CacheLine{")
            line = line.rstrip("}")

            contents = [x.split(":")[1].strip() for x in line.split(",")]
            contents = CacheLine(*contents)  # type: ignore

            cache_contents[cache_id].append(contents)

    return cache_contents


def test_modified(cache_contents, cache_id):
    """
    Test that if a cache line is in M, then it is not in M, E, S for other caches

    Args:
        cache_contents (_type_): Contents of all caches
        cache_id (_type_): Cache ID to test
    """
    try:
        modified = [
            (x.tag, x.set_index)
            for x in cache_contents[cache_id]
            if x.status == CacheStatus.MODIFIED
        ]
        modified_set = set(modified)
        assert len(modified_set) == len(modified), "Duplicate (tags, set_index)"
        del modified

        other_cache_ids = [x for x in cache_contents.keys() if x != cache_id]
        for other_cache_id in other_cache_ids:
            other_cache = cache_contents[other_cache_id]
            other_modified_set = set(
                (x.tag, x.set_index)
                for x in other_cache
                if x.status == CacheStatus.MODIFIED
            )
            intersection = modified_set.intersection(other_modified_set)
            assert len(intersection) == 0, f"M in cache {cache_id} and {other_cache_id}"

            other_exclusive_set = set(
                (x.tag, x.set_index)
                for x in other_cache
                if x.status == CacheStatus.EXCLUSIVE
            )
            intersection = modified_set.intersection(other_exclusive_set)
            assert (
                len(intersection) == 0
            ), f"M in cache {cache_id} but E in {other_cache_id}"

            other_shared_set = set(
                (x.tag, x.set_index)
                for x in other_cache
                if x.status == CacheStatus.SHARED
            )
            intersection = modified_set.intersection(other_shared_set)
            assert (
                len(intersection) == 0
            ), f"M in cache {cache_id} but S in {other_cache_id}"
    except AssertionError as e:
        print("FAIL: test_modified", e)
        raise e

    print("\tPASS: test_modified")


def test_exclusive(cache_contents, cache_id):
    """
    Test that if a cache line is in E, then it is not in M, E, S for other caches

    Args:
        cache_contents (_type_): Contents of all caches
        cache_id (_type_): Cache ID to test
    """
    TEST_NAME = "test_exclusive"

    try:
        modified = [
            (x.tag, x.set_index)
            for x in cache_contents[cache_id]
            if x.status == CacheStatus.EXCLUSIVE
        ]
        modified_set = set(modified)
        assert len(modified_set) == len(modified), "Duplicate (tags, set_index)"
        del modified

        other_cache_ids = [x for x in cache_contents.keys() if x != cache_id]
        for other_cache_id in other_cache_ids:
            other_cache = cache_contents[other_cache_id]
            other_modified_set = set(
                (x.tag, x.set_index)
                for x in other_cache
                if x.status == CacheStatus.MODIFIED
            )
            intersection = modified_set.intersection(other_modified_set)
            assert len(intersection) == 0, f"E in cache {cache_id} and {other_cache_id}"

            other_exclusive_set = set(
                (x.tag, x.set_index)
                for x in other_cache
                if x.status == CacheStatus.EXCLUSIVE
            )
            intersection = modified_set.intersection(other_exclusive_set)
            assert (
                len(intersection) == 0
            ), f"E in cache {cache_id} but E in {other_cache_id}"

            other_shared_set = set(
                (x.tag, x.set_index)
                for x in other_cache
                if x.status == CacheStatus.SHARED
            )
            intersection = modified_set.intersection(other_shared_set)
            assert (
                len(intersection) == 0
            ), f"E in cache {cache_id} but S in {other_cache_id}"
    except AssertionError as e:
        print(f"FAIL: {TEST_NAME}", e)
        raise e

    print(f"\tPASS: {TEST_NAME}")


def test_shared(cache_contents, cache_id):
    """
    Test that if a cache line is in S, then it is not in M, E for other caches

    Args:
        cache_contents (_type_): Contents of all caches
        cache_id (_type_): Cache ID to test
    """
    TEST_NAME = "test_shared"

    try:
        modified = [
            (x.tag, x.set_index)
            for x in cache_contents[cache_id]
            if x.status == CacheStatus.SHARED
        ]
        modified_set = set(modified)
        assert len(modified_set) == len(modified), "Duplicate (tags, set_index)"
        del modified

        other_cache_ids = [x for x in cache_contents.keys() if x != cache_id]
        for other_cache_id in other_cache_ids:
            other_cache = cache_contents[other_cache_id]
            other_modified_set = set(
                (x.tag, x.set_index)
                for x in other_cache
                if x.status == CacheStatus.MODIFIED
            )
            intersection = modified_set.intersection(other_modified_set)
            assert len(intersection) == 0, f"S in cache {cache_id} and {other_cache_id}"

            other_exclusive_set = set(
                (x.tag, x.set_index)
                for x in other_cache
                if x.status == CacheStatus.EXCLUSIVE
            )
            intersection = modified_set.intersection(other_exclusive_set)
            assert (
                len(intersection) == 0
            ), f"S in cache {cache_id} but E in {other_cache_id}"

    except AssertionError as e:
        print(f"FAIL: {TEST_NAME}", e)
        raise e

    print(f"\tPASS: {TEST_NAME}")


def main():
    parser = argparse.ArgumentParser(description="Test cache")
    parser.add_argument("output", help="Cache Output file")
    args = parser.parse_args()

    with open(args.output, "r", encoding="utf-8") as f:
        lines = f.readlines()

    cache_contents = process_cache(lines)

    # Statistics
    print("<<<Statistics>>>")
    for cache_id, contents in cache_contents.items():
        print(f"Cache {cache_id}:")

        # Min and max last used
        min_last_used = min(contents, key=lambda x: x.last_used)
        max_last_used = max(contents, key=lambda x: x.last_used)
        print(f"\tMin last used: {min_last_used.last_used}")
        print(f"\tMax last used: {max_last_used.last_used}")
        print()

        # Number of invalid, modified, exclusive, shared
        num_modified = len([x for x in contents if x.status == CacheStatus.MODIFIED])
        num_exclusive = len([x for x in contents if x.status == CacheStatus.EXCLUSIVE])
        num_shared = len([x for x in contents if x.status == CacheStatus.SHARED])

        print(f"\tModified: {num_modified}")
        print(f"\tExclusive: {num_exclusive}")
        print(f"\tShared: {num_shared}")
        print()
        print(f"\tTotal: {len(contents)}")
        print()

    # Tests
    print("<<<Tests>>>")
    for cache_id in cache_contents.keys():
        print(f"Testing cache {cache_id}")
        test_modified(cache_contents, cache_id)
        test_exclusive(cache_contents, cache_id)
        test_shared(cache_contents, cache_id)


if __name__ == "__main__":
    main()
