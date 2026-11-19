// ============================================================================
//  Ruby-Smoke-Test: prueft, dass die eingebettete CRuby-VM vollstaendig
//  initialisiert ist.
//
//  Hintergrund: ruby_init() allein fuehrt in Ruby 3.4 die eingebauten
//  Ruby-Skripte NICHT aus. Dann fehlen Kernmethoden wie Integer#times,
//  Integer#to_i oder Array#last - und jede Skriptzeile scheitert mit einem
//  unverstaendlichen "undefined method '#<Symbol:0x...>'".
//  Genau dafuer muss rb_call_builtin_inits() aufgerufen werden.
//
//  Bauen:
//    gcc tools/ruby_smoke.cpp -I<ruby-include> -L<ruby-lib> -lx64-ucrt-ruby340
// ============================================================================
#include <ruby.h>
#include <ruby/version.h>

#include <cstdio>
#include <string>

extern "C" void rb_call_builtin_inits(void);

namespace {

int g_failures = 0;

const char* cstr(VALUE v) {
    VALUE s = rb_obj_as_string(v);
    return rb_string_value_cstr(&s);
}

void check(const char* code, const char* expected) {
    int state = 0;
    VALUE r = rb_eval_string_protect(code, &state);
    if (state != 0) {
        VALUE m = rb_funcall(rb_errinfo(), rb_intern("message"), 0);
        std::printf("  FEHLER  %-28s -> %s\n", code, cstr(m));
        rb_set_errinfo(Qnil);
        ++g_failures;
        return;
    }
    VALUE s = rb_obj_as_string(r);
    const char* got = rb_string_value_cstr(&s);
    const bool ok = (std::string(got) == expected);
    std::printf("  %s  %-28s -> %s%s\n", ok ? "ok    " : "FALSCH", code, got,
                ok ? "" : (std::string(" (erwartet: ") + expected + ")").c_str());
    if (!ok) ++g_failures;
}

} // namespace

int main(int argc, char** argv) {
    ruby_sysinit(&argc, &argv);
    {
        RUBY_INIT_STACK;
        ruby_init();
    }
    ruby_init_loadpath();
    ruby_script("rune_smoke");

    // Der eigentliche Fixpunkt:
    rb_call_builtin_inits();

    check("RUBY_VERSION", ruby_version);
    check("Integer.instance_methods(false).size.to_s", "65");
    check("18.to_i.to_s", "18");
    check("18.times { |i| i }.to_s", "18");
    check("18.even?.to_s", "true");
    check("[1,2,3].last(2).inspect", "[2, 3]");
    check("'abc'.upcase", "ABC");
    check("(1..4).sum.to_s", "10");
    check("({a: 1}).size.to_s", "1");

    ruby_finalize();

    std::printf("%s (%d Fehler)\n", g_failures == 0 ? "SMOKE-TEST OK" : "SMOKE-TEST FEHLGESCHLAGEN",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
