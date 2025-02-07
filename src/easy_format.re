open Format;

let rev_split = (l) => {
  let rec inner = (xs, ys) =>
    fun
    | [(x, y), ...xys] => inner([x, ...xs], [y, ...ys], xys)
    | [] => (xs, ys);
  inner([], [], l);
};

type wrap = [
  | `Wrap_atoms
  | `Always_wrap
  | `Never_wrap
  | `Force_breaks
  | `Force_breaks_rec
  | `No_breaks
];

type label_break = [ | `Auto | `Always | `Always_rec | `Never];

type style_name = string;

type style = {
  tag_open: string,
  tag_close: string
};

type atom_param = {atom_style: option(style_name)};

let atom = {atom_style: None};

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

let list = {
  space_after_opening: true,
  space_after_separator: true,
  space_before_separator: false,
  separators_stick_left: true,
  space_before_closing: true,
  stick_to_label: true,
  align_closing: true,
  wrap_body: `Wrap_atoms,
  indent_body: 2,
  list_style: None,
  opening_style: None,
  body_style: None,
  separator_style: None,
  closing_style: None
};

type label_param = {
  label_break,
  space_after_label: bool,
  indent_after_label: int,
  label_style: option(style_name)
};

let label = {
  label_break: `Auto,
  space_after_label: true,
  indent_after_label: 2,
  label_style: None
};

type t =
  | Atom(string, atom_param)
  | List((string, string, string, list_param), list(t))
  | Label((t, label_param), t)
  | Custom(formatter => unit);

type escape = [
  | `None
  | `Escape(((string, int, int) => unit, string, int, int) => unit)
  | `Escape_string(string => string)
];

type styles = list((style_name, style));

/*
    Transform a tree starting from the leaves, propagating and merging
    accumulators until reaching the root.
 */
let propagate_from_leaf_to_root =
    (
      ~init_acc, /* create initial accumulator for a leaf */
      ~merge_acc, /* merge two accumulators coming from child nodes */
      ~map_node, /* (node, acc) -> (node, acc) */
      x
    ) => {
  let rec aux = (x) =>
    switch x {
    | Atom(_) =>
      let acc = init_acc(x);
      map_node(x, acc);
    | List(param, children) =>
      let (new_children, accs) = rev_split(List.rev_map(aux, children));
      let acc = List.fold_left(merge_acc, init_acc(x), accs);
      map_node([@implicit_arity] List(param, new_children), acc);
    | Label((x1, param), x2) =>
      let acc0 = init_acc(x);
      let (new_x1, acc1) = aux(x1);
      let (new_x2, acc2) = aux(x2);
      let acc = merge_acc(merge_acc(acc0, acc1), acc2);
      map_node([@implicit_arity] Label((new_x1, param), new_x2), acc);
    | Custom(_) =>
      let acc = init_acc(x);
      map_node(x, acc);
    };
  aux(x);
};

/*
    Convert wrappable lists into vertical lists if any of their descendants
    has the attribute wrap_body = `Force_breaks_rec.
 */
let propagate_forced_breaks = (x) => {
  /* acc = whether to force breaks in wrappable lists or labels */
  let init_acc =
    fun
    | List((_, _, _, {wrap_body: `Force_breaks_rec, _}), _)
    | Label((_, {label_break: `Always_rec, _}), _) => true
    | Atom(_)
    | Label(_)
    | Custom(_)
    | List(_) => false;
  let merge_acc = (force_breaks1, force_breaks2) => force_breaks1 || force_breaks2;
  let map_node = (x, force_breaks) =>
    switch x {
    | List((_, _, _, {wrap_body: `Force_breaks_rec, _}), _) => (x, true)
    | List((_, _, _, {wrap_body: `Force_breaks, _}), _) => (x, force_breaks)
    | [@implicit_arity]
      List((op, sep, cl, {wrap_body: `Wrap_atoms | `Never_wrap | `Always_wrap, _} as p), children) =>
      if (force_breaks) {
        let p = {...p, wrap_body: `Force_breaks};
        ([@implicit_arity] List((op, sep, cl, p), children), true);
      } else {
        (x, false);
      }
    | Label((a, {label_break: `Auto, _} as lp), b) =>
      if (force_breaks) {
        let lp = {...lp, label_break: `Always};
        ([@implicit_arity] Label((a, lp), b), true);
      } else {
        (x, false);
      }
    | List((_, _, _, {wrap_body: `No_breaks, _}), _)
    | Label((_, {label_break: `Always | `Always_rec | `Never, _}), _)
    | Atom(_)
    | Custom(_) => (x, force_breaks)
    };
  let (new_x, _forced_breaks) = propagate_from_leaf_to_root(~init_acc, ~merge_acc, ~map_node, x);
  new_x;
};

module Pretty = {
  /*
      Rewrite the tree to be printed.
      Currently, this is used only to handle `Force_breaks_rec.
   */
  let rewrite = (x) => propagate_forced_breaks(x);
  /*
     Relies on the fact that mark_open_tag and mark_close_tag
     are called exactly once before calling pp_output_string once.
     It's a reasonable assumption although not guaranteed by the
     documentation of the Format module.
   */
  let set_escape = (fmt, escape) => {
    let (print0, flush0) = pp_get_formatter_output_functions(fmt, ());
    let tagf0 = pp_get_formatter_tag_functions(fmt, ());
    let is_tag = ref(false);
    let mot = (tag) => {
      is_tag := true;
      tagf0.mark_open_tag(tag);
    };
    let mct = (tag) => {
      is_tag := true;
      tagf0.mark_close_tag(tag);
    };
    let print = (s, p, n) =>
      if (is_tag^) {
        print0(s, p, n);
        is_tag := false;
      } else {
        escape(print0, s, p, n);
      };
    let tagf = {...tagf0, mark_open_tag: mot, mark_close_tag: mct};
    pp_set_formatter_output_functions(fmt, print, flush0);
    pp_set_formatter_tag_functions(fmt, tagf);
  };
  let set_escape_string = (fmt, esc) => {
    let escape = (print, s, p, n) => {
      let s0 = String.sub(s, p, n);
      let s1 = esc(s0);
      print(s1, 0, String.length(s1));
    };
    set_escape(fmt, escape);
  };
  let define_styles = (fmt, escape, l) => {
    if (l != []) {
      pp_set_tags(fmt, true);
      let tbl1 = Hashtbl.create(2 * List.length(l));
      let tbl2 = Hashtbl.create(2 * List.length(l));
      List.iter(
        ((style_name, style)) => {
          Hashtbl.add(tbl1, style_name, style.tag_open);
          Hashtbl.add(tbl2, style_name, style.tag_close);
        },
        l
      );
      let mark_open_tag = (style_name) =>
        try (Hashtbl.find(tbl1, style_name)) {
        | Not_found => ""
        };
      let mark_close_tag = (style_name) =>
        try (Hashtbl.find(tbl2, style_name)) {
        | Not_found => ""
        };
      let tagf = {...pp_get_formatter_tag_functions(fmt, ()), mark_open_tag, mark_close_tag};
      pp_set_formatter_tag_functions(fmt, tagf);
    };
    switch escape {
    | `None => ()
    | `Escape(esc) => set_escape(fmt, esc)
    | `Escape_string(esc) => set_escape_string(fmt, esc)
    };
  };
  let pp_open_xbox = (fmt, p, indent) =>
    switch p.wrap_body {
    | `Always_wrap
    | `Never_wrap
    | `Wrap_atoms => pp_open_hvbox(fmt, indent)
    | `Force_breaks
    | `Force_breaks_rec => pp_open_vbox(fmt, indent)
    | `No_breaks => pp_open_hbox(fmt, ())
    };
  let extra_box = (p, l) => {
    let wrap =
      switch p.wrap_body {
      | `Always_wrap => true
      | `Never_wrap
      | `Force_breaks
      | `Force_breaks_rec
      | `No_breaks => false
      | `Wrap_atoms =>
        List.for_all(
          fun
          | Atom(_) => true
          | _ => false,
          l
        )
      };
    if (wrap) {
      ((fmt) => pp_open_hovbox(fmt, 0), (fmt) => pp_close_box(fmt, ()));
    } else {
      ((_) => (), (_) => ());
    };
  };
  let pp_open_nonaligned_box = (fmt, p, indent, l) =>
    switch p.wrap_body {
    | `Always_wrap => pp_open_hovbox(fmt, indent)
    | `Never_wrap => pp_open_hvbox(fmt, indent)
    | `Wrap_atoms =>
      if (List.for_all(
            fun
            | Atom(_) => true
            | _ => false,
            l
          )) {
        pp_open_hovbox(fmt, indent);
      } else {
        pp_open_hvbox(fmt, indent);
      }
    | `Force_breaks
    | `Force_breaks_rec => pp_open_vbox(fmt, indent)
    | `No_breaks => pp_open_hbox(fmt, ())
    };
  let open_tag = (fmt) =>
    fun
    | None => ()
    | Some(s) => pp_open_tag(fmt, s);
  let close_tag = (fmt) =>
    fun
    | None => ()
    | Some(_) => pp_close_tag(fmt, ());
  let tag_string = (fmt, o, s) =>
    switch o {
    | None => pp_print_string(fmt, s)
    | Some(tag) =>
      pp_open_tag(fmt, tag);
      pp_print_string(fmt, s);
      pp_close_tag(fmt, ());
    };
  let rec fprint_t = (fmt) =>
    fun
    | Atom(s, p) => tag_string(fmt, p.atom_style, s)
    | List((_, _, _, p) as param, l) => {
        open_tag(fmt, p.list_style);
        if (p.align_closing) {
          fprint_list(fmt, None, param, l);
        } else {
          fprint_list2(fmt, param, l);
        };
        close_tag(fmt, p.list_style);
      }
    | Label(label, x) => fprint_pair(fmt, label, x)
    | Custom(f) => f(fmt)
  and fprint_list_body_stick_left = (fmt, p, sep, hd, tl) => {
    open_tag(fmt, p.body_style);
    fprint_t(fmt, hd);
    List.iter(
      (x) => {
        if (p.space_before_separator) {
          pp_print_string(fmt, " ");
        };
        tag_string(fmt, p.separator_style, sep);
        if (p.space_after_separator) {
          pp_print_space(fmt, ());
        } else {
          pp_print_cut(fmt, ());
        };
        fprint_t(fmt, x);
      },
      tl
    );
    close_tag(fmt, p.body_style);
  }
  and fprint_list_body_stick_right = (fmt, p, sep, hd, tl) => {
    open_tag(fmt, p.body_style);
    fprint_t(fmt, hd);
    List.iter(
      (x) => {
        if (p.space_before_separator) {
          pp_print_space(fmt, ());
        } else {
          pp_print_cut(fmt, ());
        };
        tag_string(fmt, p.separator_style, sep);
        if (p.space_after_separator) {
          pp_print_string(fmt, " ");
        };
        fprint_t(fmt, x);
      },
      tl
    );
    close_tag(fmt, p.body_style);
  }
  and fprint_opt_label = (fmt) =>
    fun
    | None => ()
    | Some((lab, lp)) => {
        open_tag(fmt, lp.label_style);
        fprint_t(fmt, lab);
        close_tag(fmt, lp.label_style);
        if (lp.space_after_label) {
          pp_print_string(fmt, " ");
        };
      }
  /* Either horizontal or vertical list */
  and fprint_list = (fmt, label, (op, _sep, cl, p) as param) =>
    fun
    | [] => {
        fprint_opt_label(fmt, label);
        tag_string(fmt, p.opening_style, op);
        if (p.space_after_opening || p.space_before_closing) {
          pp_print_string(fmt, " ");
        };
        tag_string(fmt, p.closing_style, cl);
      }
    | [hd, ...tl] as l =>
      if (tl == [] || p.separators_stick_left) {
        fprint_list_stick_left(fmt, label, param, hd, tl, l);
      } else {
        fprint_list_stick_right(fmt, label, param, hd, tl, l);
      }
  and fprint_list_stick_left = (fmt, label, (op, sep, cl, p), hd, tl, l) => {
    let indent = p.indent_body;
    pp_open_xbox(fmt, p, indent);
    fprint_opt_label(fmt, label);
    tag_string(fmt, p.opening_style, op);
    if (p.space_after_opening) {
      pp_print_space(fmt, ());
    } else {
      pp_print_cut(fmt, ());
    };
    let (open_extra, close_extra) = extra_box(p, l);
    open_extra(fmt);
    fprint_list_body_stick_left(fmt, p, sep, hd, tl);
    close_extra(fmt);
    if (p.space_before_closing) {
      pp_print_break(fmt, 1, - indent);
    } else {
      pp_print_break(fmt, 0, - indent);
    };
    tag_string(fmt, p.closing_style, cl);
    pp_close_box(fmt, ());
  }
  and fprint_list_stick_right = (fmt, label, (op, sep, cl, p), hd, tl, l) => {
    let base_indent = p.indent_body;
    let sep_indent =
      String.length(sep)
      + (
        if (p.space_after_separator) {
          1;
        } else {
          0;
        }
      );
    let indent = base_indent + sep_indent;
    pp_open_xbox(fmt, p, indent);
    fprint_opt_label(fmt, label);
    tag_string(fmt, p.opening_style, op);
    if (p.space_after_opening) {
      pp_print_space(fmt, ());
    } else {
      pp_print_cut(fmt, ());
    };
    let (open_extra, close_extra) = extra_box(p, l);
    open_extra(fmt);
    fprint_t(fmt, hd);
    List.iter(
      (x) => {
        if (p.space_before_separator) {
          pp_print_break(fmt, 1, - sep_indent);
        } else {
          pp_print_break(fmt, 0, - sep_indent);
        };
        tag_string(fmt, p.separator_style, sep);
        if (p.space_after_separator) {
          pp_print_string(fmt, " ");
        };
        fprint_t(fmt, x);
      },
      tl
    );
    close_extra(fmt);
    if (p.space_before_closing) {
      pp_print_break(fmt, 1, - indent);
    } else {
      pp_print_break(fmt, 0, - indent);
    };
    tag_string(fmt, p.closing_style, cl);
    pp_close_box(fmt, ());
  }
  /* align_closing = false */
  and fprint_list2 = (fmt, (op, sep, cl, p)) =>
    fun
    | [] => {
        tag_string(fmt, p.opening_style, op);
        if (p.space_after_opening || p.space_before_closing) {
          pp_print_string(fmt, " ");
        };
        tag_string(fmt, p.closing_style, cl);
      }
    | [hd, ...tl] as l => {
        tag_string(fmt, p.opening_style, op);
        if (p.space_after_opening) {
          pp_print_string(fmt, " ");
        };
        pp_open_nonaligned_box(fmt, p, 0, l);
        if (p.separators_stick_left) {
          fprint_list_body_stick_left(fmt, p, sep, hd, tl);
        } else {
          fprint_list_body_stick_right(fmt, p, sep, hd, tl);
        };
        pp_close_box(fmt, ());
        if (p.space_before_closing) {
          pp_print_string(fmt, " ");
        };
        tag_string(fmt, p.closing_style, cl);
      }
  /* Printing a label:value pair.

        The opening bracket stays on the same line as the key, no matter what,
        and the closing bracket is either on the same line
        or vertically aligned with the beginning of the key.
     */
  and fprint_pair = (fmt, (lab, lp) as label, x) =>
    switch x {
    | List((op, sep, cl, p), l) when p.stick_to_label && p.align_closing =>
      fprint_list(fmt, Some(label), (op, sep, cl, p), l)
    | _ =>
      let indent = lp.indent_after_label;
      pp_open_hvbox(fmt, 0);
      open_tag(fmt, lp.label_style);
      fprint_t(fmt, lab);
      close_tag(fmt, lp.label_style);
      switch lp.label_break {
      | `Auto =>
        if (lp.space_after_label) {
          pp_print_break(fmt, 1, indent);
        } else {
          pp_print_break(fmt, 0, indent);
        }
      | `Always
      | `Always_rec =>
        pp_force_newline(fmt, ());
        pp_print_string(fmt, String.make(indent, ' '));
      | `Never =>
        if (lp.space_after_label) {
          pp_print_char(fmt, ' ');
        } else {
          ();
        }
      };
      fprint_t(fmt, x);
      pp_close_box(fmt, ());
    };
  let to_formatter = (fmt, x) => {
    let x = rewrite(x);
    fprint_t(fmt, x);
    pp_print_flush(fmt, ());
  };
  let to_buffer = (~escape=`None, ~styles=[], buf, x) => {
    let fmt = Format.formatter_of_buffer(buf);
    define_styles(fmt, escape, styles);
    to_formatter(fmt, x);
  };
  let to_string = (~escape=?, ~styles=?, x) => {
    let buf = Buffer.create(500);
    to_buffer(~escape?, ~styles?, buf, x);
    Buffer.contents(buf);
  };
  let to_channel = (~escape=`None, ~styles=[], oc, x) => {
    let fmt = formatter_of_out_channel(oc);
    define_styles(fmt, escape, styles);
    to_formatter(fmt, x);
  };
  let to_stdout = (~escape=?, ~styles=?, x) => to_channel(~escape?, ~styles?, stdout, x);
  let to_stderr = (~escape=?, ~styles=?, x) => to_channel(~escape?, ~styles?, stderr, x);
};

module Compact = {
  open Printf;
  let rec fprint_t = (buf) =>
    fun
    | Atom(s, _) => Buffer.add_string(buf, s)
    | List(param, l) => fprint_list(buf, param, l)
    | Label(label, x) => fprint_pair(buf, label, x)
    | Custom(f) => {
        /* Will most likely not be compact */
        let fmt = formatter_of_buffer(buf);
        f(fmt);
        pp_print_flush(fmt, ());
      }
  and fprint_list = (buf, (op, sep, cl, _)) =>
    fun
    | [] => bprintf(buf, "%s%s", op, cl)
    | [x, ...tl] => {
        Buffer.add_string(buf, op);
        fprint_t(buf, x);
        List.iter(
          (x) => {
            Buffer.add_string(buf, sep);
            fprint_t(buf, x);
          },
          tl
        );
        Buffer.add_string(buf, cl);
      }
  and fprint_pair = (buf, (label, _), x) => {
    fprint_t(buf, label);
    fprint_t(buf, x);
  };
  let to_buffer = (buf, x) => fprint_t(buf, x);
  let to_string = (x) => {
    let buf = Buffer.create(500);
    to_buffer(buf, x);
    Buffer.contents(buf);
  };
  let to_formatter = (fmt, x) => {
    let s = to_string(x);
    Format.fprintf(fmt, "%s", s);
    pp_print_flush(fmt, ());
  };
  let to_channel = (oc, x) => {
    let buf = Buffer.create(500);
    to_buffer(buf, x);
    Buffer.output_buffer(oc, buf);
  };
  let to_stdout = (x) => to_channel(stdout, x);
  let to_stderr = (x) => to_channel(stderr, x);
};

/* Obsolete */
module Param = {
  let list_true = {
    space_after_opening: true,
    space_after_separator: true,
    space_before_separator: true,
    separators_stick_left: true,
    space_before_closing: true,
    stick_to_label: true,
    align_closing: true,
    wrap_body: `Wrap_atoms,
    indent_body: 2,
    list_style: None,
    opening_style: None,
    body_style: None,
    separator_style: None,
    closing_style: None
  };
  let list_false = {
    space_after_opening: false,
    space_after_separator: false,
    space_before_separator: false,
    separators_stick_left: false,
    space_before_closing: false,
    stick_to_label: false,
    align_closing: false,
    wrap_body: `Wrap_atoms,
    indent_body: 2,
    list_style: None,
    opening_style: None,
    body_style: None,
    separator_style: None,
    closing_style: None
  };
  let label_true = {
    label_break: `Auto,
    space_after_label: true,
    indent_after_label: 2,
    label_style: None
  };
  let label_false = {
    label_break: `Auto,
    space_after_label: false,
    indent_after_label: 2,
    label_style: None
  };
};
