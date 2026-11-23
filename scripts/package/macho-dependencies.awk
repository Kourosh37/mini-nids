# otool -L repeats an unindented filename/architecture header for every universal
# slice. Only indented install-name records are dependencies, not those headers.
/^[ \t]+.* \(compatibility version / {
    sub(/^[ \t]+/, "")
    sub(/ \(compatibility version .*/, "")
    print
}
