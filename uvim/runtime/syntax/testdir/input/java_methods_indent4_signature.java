// MNV_TEST_SETUP let g:java_highlight_functions = 'indent4'
// MNV_TEST_SETUP let g:java_highlight_signature = 1
// MNV_TEST_SETUP set encoding=utf-8 termencoding=utf-8
import java.lang.annotation.ElementType;
import java.lang.annotation.Target;

abstract class Indent4$MethodsTests
{ // DO NOT retab! THIS FILE; REMEMBER ABOUT testdir/ftplugin.
    // TYPES.
    record Τʬ<α>(α a) { }

    enum E
    {
        A("a"), B("b"), C("c"), D("d"),
        E("e"), F("f"), G("g"), H("h");
        final String s;
        private E(String s) { this.s = s; }
    }

    @Target({ElementType.METHOD, ElementType.CONSTRUCTOR})
    @java.lang.annotation.Repeatable(Tɐggablɘs.class)
    @interface Tɐggablɘ
    {
        String[] value() default "";
    }

    @Target({ElementType.METHOD, ElementType.CONSTRUCTOR})
    @interface Tɐggablɘs
    {
        Tɐggablɘ[] value();
    }

    interface Stylable<Α>
    {
        default void ascii$0_() { }
        default Α μʭʭ$0_() { return null; }
    }

    // FIELDS.
    private static final Class<?> CLASS_LOCK = classLock();

    private final Object instanceLock = new Object();

    // CONSTRUCTORS.
    @Tɐggablɘ @Tɐggablɘ protected Indent4$MethodsTests() { }
    <T extends Comparable<T>> Indent4$MethodsTests(T t, Void v) { }
    private <T extends Comparable<T>> Indent4$MethodsTests(T t) { }

    // METHODS.
    @Tɐggablɘ @Tɐggablɘ abstract void ascii$0_(////////////////
                                                                );
    @Tɐggablɘ @Tɐggablɘ abstract <α, β> Τʬ<α> μʭʭ$0_(
                                @SuppressWarnings("bespoke") β b);

    @Tɐggablɘ private native void ascii$1_(/*////////////*/);
    @Tɐggablɘ private native <α, β> Τʬ<α>[] μʭʭ$1_(
                        java.util.function.Function<β, Τʬ<α>[]> ƒ);

    void Ascii$2_() { }
    <T, U extends Stylable<T>> void Μʭʭ$2_(U u) { }

    static final native synchronized void ascii$98_();
    static final native synchronized <α, β> Τʬ<α>[][] μʭʭ$98_(
                        java.util.function.Function<β, Τʬ<α>[][]> ƒ);

    @SuppressWarnings("strictfp")
    protected static final synchronized strictfp void ascii$99_()
    { ascii$98_(); }

    @SuppressWarnings("strictfp") protected
    static final synchronized strictfp <α, β> Τʬ<α>[] μʭʭ$99_(
                        java.util.function.Function<β, Τʬ<α>[][]> ƒ)
    {
        return
    Indent4$MethodsTests.<α, β>μʭʭ$98_(ƒ)[0];
    }

    public static Class<?> classLock() { return Indent4$MethodsTests.class; }

    @Override @SuppressWarnings("cast")
    public String toString() { return (String) "Indent4$MethodsTests"; }
}

enum E4$
{
    @SuppressWarnings("bespoke") A("a"),
    B("b"
        /*\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/),
    C("c", new Thread(

        () -> {
    })), D("d", (java.util.function.BooleanSupplier) () -> true),
    E("e", new char[] { 'a', 'b', 'c', 'd' }), F("f", new Object() {
        transient String name = "";
        @Override public String toString() { return this.name; }
    }), //\//\//\//\//\//\//\//\//\//\//\//\//\//\//\//\//\//\//\//\//
    G("g"), @Deprecated H("h");

    final String s;
    private E4$(String s) { this.s = s; }
    private <δ> E4$(String s, δ dummy) { this(s); }

    @Override public String toString() { return name().toUpperCase(); }
}

class C4$Alias<T>
{
    final T name; C4$Alias(T name) { this.name = name; }

    class Builder
    {
        final java.util.stream.Stream.Builder<T> builder =
            java.util.stream.Stream.<T>builder();

        C4$Alias<T>.Builder add(T x)
        {
            builder.accept(x);
            return this;
        }

        java.util.stream.Stream<T> build()
        {
            return builder.<T>build();
        }
    }

    static <A> C4$Alias<A>.Builder builder(A name)
    {
        return new C4$Alias<>(name).new Builder();
    }
}
