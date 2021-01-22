static const struct FileExtensionMap {
    const char ext[11];
    const uint8_t filetype; // FileTypeEnum
} extensions[] = {
    {"ada", ADA},
    {"adb", ADA},
    {"ads", ADA},
    {"asd", LISP},
    {"asm", ASM},
    {"auk", AWK},
    {"automount", INI},
    {"awk", AWK},
    {"bash", SH},
    {"bat", BATCH},
    {"bbl", TEX},
    {"bib", BIBTEX},
    {"btm", BATCH},
    {"c++", CPLUSPLUS},
    {"cc", CPLUSPLUS},
    {"cl", LISP},
    {"clj", CLOJURE},
    {"cls", TEX},
    {"cmake", CMAKE},
    {"cmd", BATCH},
    {"coffee", COFFEESCRIPT},
    {"cpp", CPLUSPLUS},
    {"cr", RUBY},
    {"cs", CSHARP},
    {"cson", COFFEESCRIPT},
    {"css", CSS},
    {"csv", CSV},
    {"cxx", CPLUSPLUS},
    {"dart", DART},
    {"desktop", INI},
    {"di", D},
    {"diff", DIFF},
    {"doap", XML},
    {"docbook", XML},
    {"docker", DOCKER},
    {"dot", DOT},
    {"doxy", CONFIG},
    {"dterc", DTE},
    {"dtx", TEX},
    {"ebuild", SH},
    {"el", LISP},
    {"emacs", LISP},
    {"eml", MAIL},
    {"eps", POSTSCRIPT},
    {"erl", ERLANG},
    {"ex", ELIXIR},
    {"exs", ELIXIR},
    {"flatpakref", INI},
    {"frag", GLSL},
    {"gawk", AWK},
    {"gcode", GCODE},
    {"gemspec", RUBY},
    {"geojson", JSON},
    {"glsl", GLSL},
    {"glslf", GLSL},
    {"glslv", GLSL},
    {"gnuplot", GNUPLOT},
    {"go", GO},
    {"gp", GNUPLOT},
    {"gperf", GPERF},
    {"gpi", GNUPLOT},
    {"groovy", GROOVY},
    {"gsed", SED},
    {"gv", DOT},
    {"hh", CPLUSPLUS},
    {"hpp", CPLUSPLUS},
    {"hrl", ERLANG},
    {"hs", HASKELL},
    {"htm", HTML},
    {"html", HTML},
    {"hxx", CPLUSPLUS},
    {"ini", INI},
    {"ins", TEX},
    {"java", JAVA},
    {"js", JAVASCRIPT},
    {"json", JSON},
    {"ksh", SH},
    {"kt", KOTLIN},
    {"kts", KOTLIN},
    {"latex", TEX},
    {"lsp", LISP},
    {"ltx", TEX},
    {"lua", LUA},
    {"m4", M4},
    {"mak", MAKE},
    {"make", MAKE},
    {"markdown", MARKDOWN},
    {"mawk", AWK},
    {"md", MARKDOWN},
    {"mk", MAKE},
    {"mkd", MARKDOWN},
    {"mkdn", MARKDOWN},
    {"ml", OCAML},
    {"mli", OCAML},
    {"moon", MOONSCRIPT},
    {"mount", INI},
    {"nawk", AWK},
    {"nginx", NGINX},
    {"nginxconf", NGINX},
    {"nim", NIM},
    {"ninja", NINJA},
    {"nix", NIX},
    {"opml", XML},
    {"page", XML},
    {"patch", DIFF},
    {"path", INI},
    {"pc", PKGCONFIG},
    {"perl", PERL},
    {"php", PHP},
    {"pl", PERL},
    {"pls", INI},
    {"plt", GNUPLOT},
    {"pm", PERL},
    {"po", GETTEXT},
    {"pot", GETTEXT},
    {"pp", RUBY},
    {"proto", PROTOBUF},
    {"ps", POSTSCRIPT},
    {"py", PYTHON},
    {"py3", PYTHON},
    {"rake", RUBY},
    {"rb", RUBY},
    {"rdf", XML},
    {"re", C}, // re2c
    {"rkt", RACKET},
    {"rktd", RACKET},
    {"rktl", RACKET},
    {"rockspec", LUA},
    {"roff", ROFF},
    {"rs", RUST},
    {"rst", RST},
    {"scala", SCALA},
    {"scm", SCHEME},
    {"scss", SCSS},
    {"sed", SED},
    {"service", INI},
    {"sh", SH},
    {"sld", SCHEME},
    {"slice", INI},
    {"sls", SCHEME},
    {"socket", INI},
    {"spec", RPMSPEC},
    {"sql", SQL},
    {"ss", SCHEME},
    {"sty", TEX},
    {"supp", CONFIG},
    {"svg", XML},
    {"target", INI},
    {"tcl", TCL},
    {"tex", TEX},
    {"texi", TEXINFO},
    {"texinfo", TEXINFO},
    {"timer", INI},
    {"toml", TOML},
    {"topojson", JSON},
    {"tr", ROFF},
    {"ts", TYPESCRIPT},
    {"tsv", TSV},
    {"tsx", TYPESCRIPT},
    {"ui", XML},
    {"vala", VALA},
    {"vapi", VALA},
    {"vcard", VCARD},
    {"vcf", VCARD},
    {"ver", VERILOG},
    {"vert", GLSL},
    {"vh", VHDL},
    {"vhd", VHDL},
    {"vhdl", VHDL},
    {"vim", VIML},
    {"wsgi", PYTHON},
    {"xhtml", HTML},
    {"xml", XML},
    {"xsd", XML},
    {"xsl", XML},
    {"xslt", XML},
    {"yaml", YAML},
    {"yml", YAML},
    {"zig", ZIG},
    {"zsh", SH},
};

static FileTypeEnum filetype_from_extension(const StringView sv)
{
    if (sv.length >= sizeof(extensions[0].ext)) {
        return NONE;
    }

    if (sv.length == 1) {
        switch (sv.data[0]) {
        case '1': case '2': case '3':
        case '4': case '5': case '6':
        case '7': case '8': case '9':
            return ROFF;
        case 'c': case 'h':
            return C;
        case 'C': case 'H':
            return CPLUSPLUS;
        case 'S': case 's':
            return ASM;
        case 'd': return D;
        case 'l': return LEX;
        case 'm': return OBJC;
        case 'v': return VERILOG;
        case 'y': return YACC;
        }
        return NONE;
    }

    const struct FileExtensionMap *e = BSEARCH(&sv, extensions, ft_compare);
    return e ? e->filetype : NONE;
}
