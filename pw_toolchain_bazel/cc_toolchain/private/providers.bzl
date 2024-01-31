# Copyright 2023 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""All shared providers that act as an API between toolchain-related rules."""

load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    "ActionConfigInfo",
    "EnvEntryInfo",
    "EnvSetInfo",
    "FlagGroupInfo",
    "ToolInfo",
    "WithFeatureSetInfo",
)
load("//actions:providers.bzl", "ActionNameInfo", "ActionNameSetInfo")

visibility(["//cc_toolchain", "//cc_toolchain/tests/..."])

# Note that throughout this file, we never use a list. This is because mutable
# types cannot be stored in depsets. Thus, we type them as a sequence in the
# provider, and convert them to a tuple in the constructor to ensure
# immutability.

# To reduce the number of require pw_cc_action_config rules, a
# pw_cc_action_config provides a list of ActionConfigInfo providers rather than
# a simpler 1:1 mapping.
PwActionConfigListInfo = provider(
    doc = "A provider containing a list of ActionConfigInfo providers.",
    fields = {
        "action_configs": "List[ActionConfigInfo]: A list of ActionConfigInfo providers.",
    },
)

PwActionNameInfo = ActionNameInfo
PwActionNameSetInfo = ActionNameSetInfo

PwFlagGroupInfo = FlagGroupInfo
PwFlagSetInfo = provider(
    doc = "A type-safe version of @bazel_tools's FlagSetInfo",
    fields = {
        "label": "Label: The label that defined this flag set. Put this in error messages for easy debugging",
        "actions": "Sequence[str]: The set of actions this is associated with",
        "implied_by_any": "Sequence[FeatureConstraintInfo]: This will be enabled if any of the listed predicates are met. Equivalent to with_features",
        "flag_groups": "Sequence[FlagGroupInfo]: Set of flag groups to include.",
    },
)

PwEnvEntryInfo = EnvEntryInfo
PwEnvSetInfo = EnvSetInfo

PwFeatureInfo = provider(
    doc = "A type-safe version of @bazel_tools's FeatureInfo",
    fields = {
        "label": "Label: The label that defined this feature. Put this in error messages for easy debugging",
        "name": "str: The name of the feature",
        "enabled": "bool: Whether this feature is enabled by default",
        "flag_sets": "depset[FlagSetInfo]: Flag sets enabled by this feature",
        "env_sets": "depset[EnvSetInfo]: Env sets enabled by this feature",
        "implies_features": "depset[FeatureInfo]: Set of features implied by this feature",
        "implies_action_configs": "depset[ActionConfigInfo]: Set of action configs enabled by this feature",
        "requires_any_of": "Sequence[FeatureSetInfo]: A list of feature sets, at least one of which is required to enable this feature. This is semantically equivalent to the requires attribute of rules_cc's FeatureInfo",
        "provides": "Sequence[str]: Indicates that this feature is one of several mutually exclusive alternate features.",
    },
)
PwFeatureSetInfo = provider(
    doc = "A type-safe version of @bazel_tools's FeatureSetInfo",
    fields = {
        "features": "depset[FeatureInfo]: The set of features this corresponds to",
    },
)
PwFeatureConstraintInfo = WithFeatureSetInfo

PwActionConfigInfo = ActionConfigInfo
PwToolInfo = ToolInfo
