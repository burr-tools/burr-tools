# FLTK GUI: widget ownership

- FLTK widgets constructed between an `Fl_Group`'s `begin()` and `end()` are automatically adopted by the enclosing group.
- Do not manually `delete` FLTK child widgets owned by groups; the parent destructor handles them.
