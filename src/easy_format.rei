/**
   Easy_format: indentation made easy.
*/;

/**
  This module provides a functional, simplified layer over
  the Format module of the standard library.

  Input data must be first modelled as a tree using 3 kinds of nodes:
  - atoms
  - lists
  - labelled nodes

  Atoms represent any text that is guaranteed to be printed as-is.
  Lists can model any sequence of items such as arrays of data
  or lists of definitions that are labelled with something
  like "int main", "let x =" or "x:".
*/;

/** List wrapping conditions:
    - [`Wrap_atoms]: wrap if the list contains only atoms
    - [`Always_wrap]: always wrap when needed
    - [`Never_wrap]: never wrap,
      i.e. the list is either horizontal or vertical
    - [`Force_breaks]: align vertically,
      i.e. always break line between list items and
      align the left edge of each item.
    - [`Force_breaks_rec]: same as [`Force_breaks] but turns
      any wrappable ancestor node's wrap property ([`Wrap_atoms]
      or [`Always_wrap]) into [`Force_breaks].
    - [`No_breaks]: align horizontally,
      i.e. never break line between list items
*/
type wrap = [
  | `Wrap_atoms
  | `Always_wrap
  | `Never_wrap
  | `Force_breaks
  | `Force_breaks_rec
  | `No_breaks
];

/** When to break the line after a [Label]:
    - [Auto]: break after the label if there's not enough room
    - [Always]: always break after the label
    - [Always_rec]: always break after the label and force breaks in all parent
      lists and labels, similarly to [`Force_breaks_rec] for lists.
    - [Never]: never break after the label
*/
type label_break = [ | `Auto | `Always | `Always_rec | `Never];

type style_name = string;

/** Pair of opening and closing tags that are inserted around
  text after pretty-printing. */
type style = {
  tag_open: string,
  tag_close: string
};

type atom_param = {atom_style: option(style_name)};

let atom: atom_param;

/** List-formatting parameters.
    Always derive a new set of parameters from an existing record.
    See {!Easy_format.list}.
*/
type list_param = {
  space_after_opening: bool,
  space_after_separator: bool,
  space_before_separator: bool,
  separators_stick_left: bool,
  space_before_closing: bool,
  stick_to_label: bool,
  align_closing: bool,
  wrap_body: wrap,
  indent_body: int,
  list_style: option(style_name),
  opening_style: option(style_name),
  body_style: option(style_name),
  separator_style: option(style_name),
  closing_style: option(style_name)
};

/** Default list-formatting parameters, using the default values
      described in the type definition above.

      In order to make code compatible with future versions of the library,
      the record inheritance syntax should be used, e.g.
      [ { list with align_closing = false } ].
      If new record fields are added, the program would still compile
      and work as before.
  */
let list: list_param;

/** Label-formatting parameters.
    Always derive a new set of parameters from an existing record.
    See {!Easy_format.label}.
*/
type label_param = {
  label_break,
  space_after_label: bool,
  indent_after_label: int,
  label_style: option(style_name)
};

/** Default label-formatting parameters, using the default values
      described in the type definition above.

      In order to make code compatible with future versions of the library,
      the record inheritance syntax should be used, e.g.
      [ { label with indent_after_label = 0 } ].
      If new record fields are added, the program would still compile
      and work as before.
 */
let label: label_param;

/** The type of the tree to be pretty-printed. Each node contains
    its own formatting parameters.

    Detail of a list node
    [List ((opening, separator, closing, param), nodes)]:

    - [opening]: opening string such as [""] [""] ["("] ["begin"] [""] etc.
    - [separator]: node separator such as [";"] [","] [""] ["+"] ["|"] etc.
    - [closing]: closing string such as [""] [""] [")"] ["end"] [""] etc.
    - [nodes]: elements of the list.

*/
type t =
  /** Plain string normally
            without line breaks. */ | Atom(string, atom_param)
  /** [List ((opening, separator, closing, param), nodes)] */
  | List((string, /* opening */ string, /* separator */ string, /* closing */ list_param), list(t))
  /** [Label ((label, param), node)]: labelled node. */ | Label((t, label_param), t)

    /** User-defined printing function that allows to use the
    Format module directly if necessary. It is responsible
    for leaving the formatter in a clean state. */

  | Custom(Format.formatter => unit);

type escape = [
  | `None
  | `Escape(((string, int, int) => unit, string, int, int) => unit)
  | `Escape_string(string => string)
];

type styles = list((style_name, style));

/** The regular pretty-printing functions */
module Pretty: {
  let define_styles: (Format.formatter, escape, styles) => unit;
  let to_formatter: (Format.formatter, t) => unit;
  let to_buffer: (~escape: escape=?, ~styles: styles=?, Buffer.t, t) => unit;
  let to_string: (~escape: escape=?, ~styles: styles=?, t) => string;
  let to_channel: (~escape: escape=?, ~styles: styles=?, out_channel, t) => unit;
  let to_stdout: (~escape: escape=?, ~styles: styles=?, t) => unit;
  let to_stderr: (~escape: escape=?, ~styles: styles=?, t) => unit;
};

/** No spacing or newlines other than those in the input data
    or those produced by [Custom] printing. */
module Compact: {
  let to_buffer: (Buffer.t, t) => unit;
  let to_string: t => string;
  let to_channel: (out_channel, t) => unit;
  let to_stdout: t => unit;
  let to_stderr: t => unit;
  let to_formatter: (Format.formatter, t) => unit;
};

/**/;

/** Deprecated. Predefined sets of parameters */
module Param: {
  /** Deprecated. All boolean fields set to true. indent_body = 2. */
  let list_true: list_param;
  /** Deprecated. All boolean fields set to true. indent_after_label = 2. */
  let label_true: label_param;
  /** Deprecated. All boolean fields set to false. indent_body = 2. */
  let list_false: list_param;
  /** Deprecated. All boolean fields set to false. indent_after_label = 2. */
  let label_false: label_param;
};
