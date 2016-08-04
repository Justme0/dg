#!/usr/bin/env ruby

require 'optparse'

options = {}
OptionParser.new do |opts|
  opts.banner = "Usage: #{File.basename __FILE__} options"

  opts.on("-c", "--criterion CRITERION", "Specify slicing criterion") do |criterion|
    options[:criterion] = criterion
  end

  opts.on("-d", "--diff", "Show difference between original and sliced IR") do
    options[:diff] = true
  end

  opts.on("-f", "--file FILE", "Specify a file to be sliced") do |src|
    options[:src] = src
  end

  opts.on_tail("-h", "--help", "Show this message") do
    puts opts
    exit
  end
end.parse!

class String
  def black;          "\e[30m#{self}\e[0m" end
  def red;            "\e[31m#{self}\e[0m" end
  def green;          "\e[32m#{self}\e[0m" end
  def brown;          "\e[33m#{self}\e[0m" end
  def blue;           "\e[34m#{self}\e[0m" end
  def magenta;        "\e[35m#{self}\e[0m" end
  def cyan;           "\e[36m#{self}\e[0m" end
  def gray;           "\e[37m#{self}\e[0m" end

  def bg_black;       "\e[40m#{self}\e[0m" end
  def bg_red;         "\e[41m#{self}\e[0m" end
  def bg_green;       "\e[42m#{self}\e[0m" end
  def bg_brown;       "\e[43m#{self}\e[0m" end
  def bg_blue;        "\e[44m#{self}\e[0m" end
  def bg_magenta;     "\e[45m#{self}\e[0m" end
  def bg_cyan;        "\e[46m#{self}\e[0m" end
  def bg_gray;        "\e[47m#{self}\e[0m" end

  def bold;           "\e[1m#{self}\e[21m" end
  def italic;         "\e[3m#{self}\e[23m" end
  def underline;      "\e[4m#{self}\e[24m" end
  def blink;          "\e[5m#{self}\e[25m" end
  def reverse_color;  "\e[7m#{self}\e[27m" end
end

src = options[:src]                  # e.g. main.c
base = File.basename("#{src}", ".*") # main
ir_ll = base + ".ll"                 # main.ll
ir_bc = base + ".bc"                 # main.bc
slice_ll = base + "_slice.ll"        # main_slice.ll
slice_bc = base + "_slice.bc"        # main_slice.bc

puts "Compiling #{src} to #{ir_ll}...".brown
`clang -S -g -emit-llvm #{src} -o #{ir_ll}`
abort unless $?.success?
puts "Done.\n".green

puts "Compiling #{src} to #{ir_bc}...".brown
`clang -c -g -emit-llvm #{src} -o #{ir_bc}`
puts "Done.\n".green

puts "Slicing #{ir_bc} to #{slice_bc}...".brown
system("./llvm-slicer -c #{options[:criterion]} #{ir_bc}")
abort unless $?.success?
puts "Done.\n".green

# get sliced IR
`cp -f #{base}.sliced #{slice_bc}`
`llvm-dis #{slice_bc} -o #{slice_ll}`
abort unless $?.success?

puts "All finished.".green
puts "source file       : #{src}"
puts "IR before slicing : #{ir_ll} #{ir_bc}"
puts "IR after slicing  : #{slice_ll} #{slice_bc}"

if options[:diff]
  system("vimdiff #{ir_ll} #{slice_ll}")
end
