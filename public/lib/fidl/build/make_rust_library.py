#!/usr/bin/env python
#
# Copyright 2016 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Make a Rust crate for a set of generated fidl files.
"""

import ast
import optparse
import os
import sys
import zipfile

from label_to_crate import label_to_crate

# Creates the directory containing the given file.
def create_base_directory(file):
    path = os.path.dirname(file)
    try:
        os.makedirs(path)
    except os.error:
        # Already existed.
        pass

def make_rust_library(output, gen_dir, inputs, srcroot, dep_inputs):
  name = os.path.basename(output)
  target_dir = os.path.dirname(output)
  longname = label_to_crate(output)
  try:
      os.makedirs(target_dir)
  except os.error:
      # Already existed.
      pass

  lib_fn = os.path.join(gen_dir, target_dir, '%s.rs' % longname)
  src_f = file(lib_fn, 'w')
  src_f.write('// Autogenerated by //garnet/public/lib/fidl/build/make_rust_library.py\n')
  src_f.write('#[macro_use] extern crate fidl;\n')
  src_f.write('extern crate fuchsia_async;\n')
  src_f.write('extern crate fuchsia_zircon as zircon;\n')
  src_f.write('#[macro_use] extern crate futures;\n')
  for dep in dep_inputs:
    crate = label_to_crate(dep)
    # TODO: distinguish deps from public_deps, and only do "pub" for the latter.
    src_f.write('pub extern crate %s;\n' % crate)
  src_f.write('\n')
  for i in inputs:
    rel = os.path.relpath(i, os.path.dirname(lib_fn))
    basename = os.path.splitext(os.path.basename(i))[0]
    if rel != basename + '.rs':
      src_f.write('#[path="%s"]\n' % rel)
    src_f.write('pub mod %s;\n' % basename)
  src_f.write('\n')
  for i in inputs:
    basename = os.path.splitext(os.path.basename(i))[0]
    src_f.write('pub use %s::*;\n' % basename)

def main():
  parser = optparse.OptionParser()

  parser.add_option('--srcroot', help='Location of source root.')
  parser.add_option('--inputs', help='List of source files for the library.')
  parser.add_option('--dep-inputs', help='List of dependencies.')
  parser.add_option('--output', help='Path to output directory for library.')
  parser.add_option('--gen-dir', help='Path to root of gen directory.')

  options, _ = parser.parse_args()

  inputs = []
  if (options.inputs):
    inputs = ast.literal_eval(options.inputs)
  dep_inputs = []
  if options.dep_inputs:
    dep_inputs = ast.literal_eval(options.dep_inputs)
  output = options.output
  gen_dir = options.gen_dir

  make_rust_library(output, gen_dir, inputs, options.srcroot, dep_inputs)

if __name__ == '__main__':
  sys.exit(main())
