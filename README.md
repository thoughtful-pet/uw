# UW library

## Achtung!

AI-generated stuff. Do not use unless you're a dog.

This software is full of fleas, meow.

## Building

The following environment variables are honoured by cmake:

* `DEBUG`: debug build (XXX not fully implementeded in cmake yet)
* `UW_WITHOUT_ICU`: if defined (the value does not matter), build without ICU dependency

## Notes

Although C++ could be a better choice, modern C provides a couple of amazing features:

1. generics
2. cleanup attribute for variables

Generics allow to mimic overloaded functions, i.e.
```c
uw_list_append(1);
uw_list_append("wow");
```

Cleanup attribute facilitates dynamic memory management.
However, it's not as good as destructors in C++ because
cleanup handlers are fired only at scope exit and
there's always a chance to shoot in the foot assigning
new value to autocleaned variable without explicitly freeing
previous value.

The general rule is to use autocleaned variables in small scopes,
especially if such variables are used in a loop body:
```c
for (unsigned i = 0, n = uw_list_length(mylist); i < n; i++) {
    {
        UwValue value = uw_list_item(mylist, i);

        // process item

        // without nested scope we'd have to explicitly
        // call uw_destroy(&item)
    }
}
```
Always initialize autocleaned variable for cleanup function
to work properly:
```
{
    UwValue myvar1 = UwUnsigned(123);
    UwValue myvar2 = UwNull();  // maybe create other value later

    // do the job
}
```

When returning a value from an autocleaned variable,
use `uw_move()` function that transfers ownership:
```
UwResult myfunc()
{
    UwValue result = UwBool(false);

    // do the job

    return uw_move(&result);
}
```

UwList(...) and UwMap(...) are macros that terminate va_list with UwVaEnd().
Values can be created during the call, and constructors make sure that arguments do not
contain values of Status type.

If, however, an argument is a status, it is returned as is and other arguments are destroyed.


Yes, C is weird. C++ could handle this better, but it's weird in its own way.

## COW rules for strings

* if a string is copied, only refcount is incremented
* if a string is about to be modified and refcount is 1, it is modified in place.
* if a string is about to be modified and refcount is more than 1, a copy is created
  with refcount 1 and then modified in place.
* string capacity is preserved on copy
