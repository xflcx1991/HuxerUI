# Declarative UI DSL Design

Status: initial implementation

HuxerUI includes an optional `.ui` front end for small declarative component trees. The front end is build-time syntax sugar: it generates ordinary C++ that uses `Scope`, `UseState`, `Row`, `Column`, `Stack`, `Text`, `Button`, and existing modifiers. It does not add a second Runtime, layout engine, or state system.

## Example

```text
component Counter {
  state {
    count: 0
  }

  Center {
    Row {
      spacing: 12
      crossAxisAlignment: center

      Button {
        text: "-"
        enabled: count > 0
        onClick: count = count - 1
      }
      Text {
        text: "${count}"
        fontSize: 30
      }
      Button {
        text: "+"
        enabled: count < 100
        onClick: count = count + 1
      }
    }
  }
}
```

`state` declarations become `UseState` values in a generated `Scope`. Reading a state in an expression or `${name}` interpolation subscribes the component through the existing state mechanism. An `onClick` assignment writes the same state value. `Center` is a convenience node lowered to a `Stack` with a centered `Align` modifier.

The layout model follows HuxerUI's existing constraint-based layouts. `Row`, `Column`, `Stack`, spacing, alignment, padding, and grow are preferred over references such as `$screen.width` or `parent.width`; a component should not need to read its parent to participate in normal layout.

## Build integration

```cmake
huxerui_add_declarative(
    my_app
    SOURCES counter.ui
)
```

The command generates `<name>.generated.cpp` and `<name>.generated.h` in the build tree, adds them to the target, and adds the generated include directory. The generated component declarations live in `huxerui_generated` by default; pass `NAMESPACE` to select another namespace.

Native source builds use the `huxerui_declarative_codegen` host executable. Cross-compiling builds must set `HUXERUI_DECLARATIVE_CODEGEN` to a generator executable built for the development host.

## Initial limits

The initial grammar supports component declarations, nested `Center`/`Text`/`Button`/`Row`/`Column`/`Stack` nodes, scalar state literals, arithmetic and boolean expressions, text interpolation, and single-state assignments from `onClick`. Text and button labels are double-quoted strings. Parent-size shortcuts, arbitrary C++ expressions, loops, conditional children, and custom components are not part of this first version.
