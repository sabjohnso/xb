# Railroad Diagrams

xb can render XML Schema content models as railroad (syntax) diagrams in SVG.
These visual representations show the structure of complex types — sequences,
choices, optional and repeating elements — in an intuitive graphical format.

## Usage

```sh
xb railroad [OPTIONS] SCHEMA...
```

### Options

| Option | Description |
|--------|-------------|
| `--type NAME` | Target complex type local name |
| `--element NAME` | Target root element (discovers reachable types) |
| `--namespace URI` | Namespace URI of the target |
| `-o`, `--output FILE` | Output SVG file (default: stdout) |
| `--theme THEME` | Color scheme: `light` (default) or `dark` |
| `--transparent` | Use a transparent background instead of a solid fill |

### Examples

```sh
# Diagram for a specific type
xb railroad --type Address -o address.svg schema.xsd

# Diagram for a root element and all reachable types
xb railroad --element purchaseOrder -o po.svg schema.xsd

# From RELAX NG
xb railroad --element addressbook -o book.svg schema.rnc
```

## Example Output

The following diagram was generated from an addressbook schema with
`xb railroad --theme light --transparent --element addressbook addressbook.xsd`:

![Addressbook railroad diagram](addressbook-railroad-light.svg#only-light)
![Addressbook railroad diagram](addressbook-railroad-dark.svg#only-dark)

The diagram shows three types discovered from the root element. Each type's
content model reads left to right: required elements appear inline, optional
elements have a bypass path above, and repeating elements have a loop-back
arrow. Attributes are listed below the element track.

Use `--theme dark --transparent` to generate diagrams suitable for dark
backgrounds, or `--theme light --transparent` for light backgrounds with
no fill.

## Input Formats

Railroad diagrams work with all schema formats xb supports:

- XSD (`.xsd`)
- RELAX NG (`.rng`)
- RELAX NG Compact (`.rnc`)
- DTD (`.dtd`)

## Diagram Elements

The generated SVG uses standard railroad diagram conventions:

| Visual Element | Meaning |
|----------------|---------|
| Rounded box | Terminal (element or attribute name) |
| Straight path | Sequence (left to right) |
| Branching paths | Choice (alternatives) |
| Loop-back arrow | Repeating element (`maxOccurs > 1`) |
| Bypass path | Optional element (`minOccurs="0"`) |

## Embedding in Documentation

The SVG output can be embedded directly in HTML or Markdown:

```html
<img src="schema-diagram.svg" alt="Schema structure"/>
```

Or inline in HTML:

```html
<div class="schema-diagram">
  <!-- paste SVG content here -->
</div>
```
