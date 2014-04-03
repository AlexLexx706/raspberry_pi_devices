#include <Python.h>
#include <wiringPi.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>

static PyObject * UltrasonicError;
static int count = 0;
static int started = 0;

pthread_mutex_t count_mutex;
pthread_cond_t count_threshold_cv;

struct TempData{
    volatile uint32_t time1;
    volatile uint32_t time2;
    volatile uint32_t time_diff;
    volatile float range_cm;
    volatile int flag;
    int TRIGGER_IN;
    int ECHO_OUT;
    int ready;
    int loop;
    int stop;
    void (* fun)();
};

struct TempData data[4];


void show_distance(struct TempData * data)
{
    delay(100);
    digitalWrite(data->TRIGGER_IN, 0);
    delayMicroseconds(1);
    digitalWrite(data->TRIGGER_IN, 1);
    delayMicroseconds(10);
    digitalWrite(data->TRIGGER_IN, 0);
}


void internal_interrupt(struct TempData * data)
{
    if(data->flag==0)
    {
        data->time1 = micros();
        data->flag = 1;
    }
    else
    {
        data->time2 = micros();
        data->flag = 0;
        data->time_diff = data->time2 - data->time1;
        data->range_cm = data->time_diff / 58.;

        if (!data->loop || data->stop)
        {
            pthread_mutex_lock(&count_mutex);
            data->ready = 1;
            pthread_cond_signal(&count_threshold_cv);
            pthread_mutex_unlock(&count_mutex);
        }
        else
           show_distance(data);
    }
}


void myInterrupt1(void)
{
    internal_interrupt(&data[0]);
}

void myInterrupt2(void)
{
    internal_interrupt(&data[1]);
}

void myInterrupt3(void)
{
    internal_interrupt(&data[2]);
}

void myInterrupt4(void)
{
    internal_interrupt(&data[3]);
}

static PyObject* init(PyObject* self)
{
    if (started)
    {
        PyErr_SetString(UltrasonicError, "Module already started");
        return NULL;
    }

    if ( wiringPiSetup() < 0 )
    {
        PyErr_SetString(UltrasonicError, "WiringPiSetup failed!!!");
        return NULL;
    }
    
    memset(data, 0, sizeof(data));
    data[0].fun = myInterrupt1;
    data[1].fun = myInterrupt2;
    data[2].fun = myInterrupt3;
    data[3].fun = myInterrupt4;
    started = 1;
    
    return Py_BuildValue("");
}

static PyObject* connect(PyObject *self, PyObject *args)
{
    if (!started)
    {
        PyErr_SetString(UltrasonicError, "Module not started");
        return NULL;
    }

    if (count > 4)
    {
        PyErr_SetString(UltrasonicError, "max sensors count");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "ii", &data[count].TRIGGER_IN, &data[count].ECHO_OUT))
    {
        return NULL;
    }

    pinMode(data[count].TRIGGER_IN,OUTPUT);
    pinMode(data[count].ECHO_OUT, INPUT);
    pullUpDnControl(data[count].ECHO_OUT, PUD_DOWN);
    
    if (wiringPiISR(data[count].ECHO_OUT, INT_EDGE_BOTH, data[count].fun) < 0)
    {
        PyErr_SetObject(UltrasonicError, Py_BuildValue("si", "interrupt error:", errno));
        return NULL;
    }

    count++;
    return Py_BuildValue("i", count - 1);
}

static PyObject* measure(PyObject *self, PyObject *args)
{
    if (!started)
    {
        PyErr_SetString(UltrasonicError, "Module not started");
        return NULL;
    }

    int id;
    double time = 1.;

    if (!PyArg_ParseTuple(args, "i|d", &id, &time))
        return NULL;

    if (id >= count)
    {
        PyErr_SetString(UltrasonicError, "Wrong id number");
        return NULL;
    }
    
    if (time < 0.035)
        time = 0.035;
    

    double fractpart, intpart;

    fractpart = modf(time , &intpart);
    
    //установим время ошибки.
    struct timespec abstime;
    clock_gettime(CLOCK_REALTIME, &abstime);
    abstime.tv_sec += intpart;
    abstime.tv_nsec += fractpart * 1e9;
    long dt = abstime.tv_nsec - 1000000000;
    
    if (dt > 0)
    {
        abstime.tv_sec++;
        abstime.tv_nsec = dt;
     }
        
    //printf("sec:%u, nsec:%u\n", abstime.tv_sec, abstime.tv_nsec);

    data[id].ready = 0;
    data[id].flag = 0;

    digitalWrite(data[id].TRIGGER_IN, 0);
    delayMicroseconds(1);
    digitalWrite(data[id].TRIGGER_IN, 1);
    delayMicroseconds(10);
    digitalWrite(data[id].TRIGGER_IN, 0);

    //ждём завершения.
    pthread_mutex_lock(&count_mutex);

    while (!data[id].ready)
    {
        //В случае таймаута возвращаем None
        if ( ETIMEDOUT == pthread_cond_timedwait(&count_threshold_cv, &count_mutex, &abstime) )
        {
            pthread_mutex_unlock(&count_mutex);
            return Py_BuildValue("");
        }
    }
    pthread_mutex_unlock(&count_mutex);

    return Py_BuildValue("f", data[id].range_cm);
}



static PyObject* measure_multiply(PyObject *self, PyObject *args)
{
    if (!started)
    {
        PyErr_SetString(UltrasonicError, "Module not started");
        return NULL;
    }

    PyObject * ids_py;
    double time = 1.;
    
    //printf("1\n");

    if (!PyArg_ParseTuple(args, "O|d", &ids_py, &time))
        return NULL;
    
    //printf("2\n");    
    
    //1. парсим лист.
    if ( !PyList_Check(ids_py) )
    {
        PyErr_SetString(UltrasonicError, "Wrong type");
        return NULL;
    }
    
    //printf("3\n");
    
    int ids[4];
    Py_ssize_t size = PyList_Size(ids_py);
    int i = 0;
    PyObject * item;
    
    //printf("4\n");
    
    for (; i < size; i++)
    {
        item = PyList_GetItem(ids_py, i);
        
        if (!PyInt_Check(item))
        {
            PyErr_SetString(UltrasonicError, "Wrong id type");
            return NULL;
        }

        ids[i] = PyInt_AS_LONG(item);

        if ( ids[i] >= count)
        {
            PyErr_SetString(UltrasonicError, "Wrong id number");
            return NULL;
        }
    }

    //printf("5\n");
    //установим время ошибки.    
    if (time < 0.035)
        time = 0.035;

    double fractpart, intpart;

    fractpart = modf(time , &intpart);
    
    struct timespec abstime;
    clock_gettime(CLOCK_REALTIME, &abstime);
    abstime.tv_sec += intpart;
    abstime.tv_nsec += fractpart * 1e9;
    long dt = abstime.tv_nsec - 1000000000;
    
    if (dt > 0)
    {
        abstime.tv_sec++;
        abstime.tv_nsec = dt;
    }
        
    //printf("sec:%u, nsec:%u\n", abstime.tv_sec, abstime.tv_nsec);

    //printf("6\n");
    
    for (i = 0; i < size; i++)
    {
        data[ids[i]].ready = 0;
        data[ids[i]].flag = 0;
        digitalWrite(data[ids[i]].TRIGGER_IN, 0);
    }
        
    delayMicroseconds(1);
    
    for (i = 0; i < size; i++)
        digitalWrite(data[ids[i]].TRIGGER_IN, 1);
        
    delayMicroseconds(10);

    for (i = 0; i < size; i++)
        digitalWrite(data[ids[i]].TRIGGER_IN, 0);

    //printf("7 size:%i\n", size);
    //ждём завершения.
    pthread_mutex_lock(&count_mutex);

    int ready = 1;
    
    for (;;)
    {
        //проверка условия
        for (i = 0; i < size; i++)
        {
            if (data[ids[i]].ready == 0)
            {
                ready = 0;
                break;
            }
        }

        //printf("8\n", size);
        
        if (ready)
            break;

        //В случае таймаута возвращаем None
        if ( ETIMEDOUT == pthread_cond_timedwait(&count_threshold_cv, &count_mutex, &abstime) )
        {
            pthread_mutex_unlock(&count_mutex);
            return Py_BuildValue("");
        }
        
        ready = 1;
    }
    pthread_mutex_unlock(&count_mutex);
    
    //printf("9\n", size);
    ids_py = PyList_New(size);
    
    for (i = 0; i < size; i++)
        PyList_SET_ITEM(ids_py, i, PyFloat_FromDouble(data[ids[i]].range_cm));

    return ids_py;
}





static PyMethodDef ultrasonic_funcs[] = {
    {"init", (PyCFunction)init, METH_NOARGS, "init() init module, need run one time\n"},
    {"connect", (PyCFunction)connect, METH_VARARGS, "connect(in, out) -> int Connect to pins: in - TRIGGER_IN, out - ECHO_OUT, WiringPi PINS.\n return: connection id - int"},
    {"measure", (PyCFunction)measure, METH_VARARGS, "measure(id, [timeout]) -> float, mesure distamse in cm.\n Params: id - from connect, [timeout] - float sec.\n Result: distance in cm float or None - error/timeout\n"},
    {"measure_multiply", (PyCFunction)measure_multiply, METH_VARARGS, "measure_multiply([id,..], [timeout])->list of float.\n Params: ids list, timeout\n Result: list of floats or None-error/timeout\n"},
    {NULL}
};

void initultrasonic(void)
{
    PyObject * m = Py_InitModule3("ultrasonic", ultrasonic_funcs, "Ultrasonic detector module");

    if (m == NULL)
        return;

    UltrasonicError = PyErr_NewException("ultrasonic.error", NULL, NULL);
    Py_INCREF(UltrasonicError);
    PyModule_AddObject(m, "error", UltrasonicError);
}