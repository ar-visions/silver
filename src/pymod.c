#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>

/* --- The 'hello' Class Definition --- */

// 1a. Define the structure for the Python 'hello' object instance
typedef struct {
    PyObject_HEAD // Required macro for Python object internals
    // We don't need any extra C members for this simple example
} HiHelloObject;

// 1b. C implementation of the 'message' method
static PyObject*
hello_message(HiHelloObject *self, PyObject *Py_UNUSED(ignored))
{
    // This is the string returned to Python when my_instance.message() is called
    return PyUnicode_FromString("Hello from the 'hello' class message method!");
}

// 1c. Method definitions table for the 'hello' class
static PyMethodDef HelloMethods[] = {
    // {PyName in Python, C Function Pointer, Call Convention, Docstring}
    {"message", (PyCFunction) hello_message, METH_NOARGS, "Prints a message from the hello class."},
    {NULL, NULL, 0, NULL}  /* Sentinel: Marks the end of the list */
};

// 1d. Type definition for the 'hello' class itself
static PyTypeObject HiHelloType = {
    PyObject_HEAD_INIT(NULL)
    .tp_name = "hi.hello",             // Full Python path: module_name.class_name
    .tp_doc = "Hello class documentation",
    .tp_basicsize = sizeof(HiHelloObject), // How big our C struct is
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_methods = HelloMethods,        // Link the methods defined above
    .tp_new = PyType_GenericNew,       // Standard constructor for new instances
};


/* --- 2. Module Definition and Initialization --- */

// 2a. Module definition structure
static struct PyModuleDef himodule = {
    PyModuleDef_HEAD_INIT,
    "hi",   // Name of the module when imported in Python
    "Documentation for the 'hi' module.",
    -1,     // Size of per-interpreter state
    NULL    // No module-level methods (like 'yo' was)
};

// 2b. The required initialization function (PyInit_modulename)
PyMODINIT_FUNC
PyInit_hi(void)
{
    PyObject *m;

    // Create the module object
    m = PyModule_Create(&himodule);
    if (m == NULL)
        return NULL;

    // Prepare the 'hello' type and add it to the 'hi' module
    if (PyType_Ready(&HiHelloType) < 0)
        return NULL;
    Py_INCREF(&HiHelloType); // Increment ref count because module holds a ref
    PyModule_AddObject(m, "hello", (PyObject *) &HiHelloType);

    // Return the final, fully configured module
    return m;
}
