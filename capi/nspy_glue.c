/*
 * Copyright © 2011 Christian Kellner <kellner@bio.lmu.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Christian Kellner <kellner@bio.lmu.de>
 */

#include <Python.h>
#include <numpy/arrayobject.h>
#include <numpy/ndarrayobject.h>
#include <arpa/inet.h>
#include <stdio.h>

#include <dlfcn.h>

#include "nsAPItypes.h"
#include "nsAPIdllimp.h"

static PyObject *PgError;

typedef struct {
  void              *dl_handle;

  NS_GETLIBRARYINFO       GetLibraryInfo;
  NS_OPENFILE             OpenFile;
  NS_CLOSEFILE            CloseFile;
  NS_GETFILEINFO          GetFileInfo;
  NS_GETENTITYINFO        GetEntityInfo;
  NS_GETEVENTINFO         GetEventInfo;
  NS_GETEVENTDATA         GetEventData;
  NS_GETANALOGINFO        GetAnalogInfo;
  NS_GETANALOGDATA        GetAnalogData;
  NS_GETSEGMENTINFO       GetSegmentInfo;
  NS_GETSEGMENTSOURCEINFO GetSegmentSourceInfo;
  NS_GETSEGMENTDATA       GetSegmentData;
  NS_GETNEURALINFO        GetNeuralInfo;
  NS_GETNEURALDATA        GetNeuralData;
  NS_GETINDEXBYTIME       GetIndexByTime;
  NS_GETTIMEBYINDEX       GetTimeByIndex;
  NS_GETLASTERRORMSG      GetLastErrorMsg;

} NsLibrary;

uint8
uint8_from_data (void *data, size_t data_len)
{
  uint8 ret;
  
  if (data_len == 1)
    {
      uint8 *u8 = data;
      ret = *u8;
    }
  else if (data_len == 2)
    {
      ret = *(uint8 *) data;
    }
  else if (data_len == 4)
    {
      uint32 *u32;
      u32 = (uint32 *) data;
      ret =  (uint8) *u32;
    }
  else
    {
      ret = 0;
    }
  
  return ret;
}

static uint16
uint16_from_data (void *data, size_t data_len)
{
  uint16 ret;
  
  if (data_len == 1)
    {
      uint8 *u8 = data;
      ret = (uint16) *u8;
    }
  else if (data_len == 2)
    {
      ret = *(uint16 *) data;
    }
  else if (data_len == 4)
    {
      uint32 *u32;
      u32 = (uint32 *) data;
      ret =  (uint16) *u32;
    }
  else
    {
      ret = 0;
    }
  
  return ret;
}

static uint32
uint32_from_data (void *data, size_t data_len)
{
  uint16 ret;
  
  if (data_len == 1)
    {
      uint8 *u8 = data;
      ret = (uint32) *u8;
    }
  else if (data_len == 2)
    {
      uint16 *u16;
      u16 = (uint16 *) data;
      ret =  (uint32) *u16;
    }
  else if (data_len == 4)
    {
      ret = *(uint32 *) data;
    }
  else
    {
      ret = 0;
    }
  
  return ret;
}

static int
check_result_is_error (ns_RESULT res, NsLibrary *lib)
{
  char buf[1024] = {0, };

  if (res == ns_OK)
    return 0;

  lib->GetLastErrorMsg (buf, sizeof (buf));
  PyErr_Format (PgError, "%s", buf);
  return 1;
}

static int
dict_set_item_eat_ref (PyObject *dict, const char *string, PyObject *item)
{
  int res;

  res = PyDict_SetItemString (dict, string, item);
  Py_DECREF (item);

  return res;
}

/* ************************************************************************** */

static PyObject *
library_open (PyObject *self, PyObject *args, PyObject *kwds)
{
  NsLibrary  *lib;
  PyObject   *lib_handle;
  const char *filename;
  void       *dlh;
  int         flags;

  flags = RTLD_NOW;

  if (!PyArg_ParseTuple (args, "s", &filename))
    return NULL;
  
  dlh = dlopen (filename, flags);

  if (dlh == NULL)
    {
      PyErr_Format (PgError, "Could not load library: %s", dlerror ());
      return NULL;
    }

  lib = malloc (sizeof (NsLibrary));
  lib->dl_handle = dlh;

  lib->GetLibraryInfo = dlsym (dlh, "ns_GetLibraryInfo");
  lib->OpenFile = dlsym (dlh, "ns_OpenFile");
  lib->CloseFile = dlsym (dlh, "ns_CloseFile");
  lib->GetFileInfo = dlsym (dlh, "ns_GetFileInfo");
  lib->GetEntityInfo = dlsym (dlh, "ns_GetEntityInfo");
  lib->GetEventInfo = dlsym (dlh, "ns_GetEventInfo");
  lib->GetEventData = dlsym (dlh, "ns_GetEventData");
  lib->GetAnalogInfo = dlsym (dlh, "ns_GetAnalogInfo");
  lib->GetAnalogData = dlsym (dlh, "ns_GetAnalogData");
  lib->GetSegmentInfo = dlsym (dlh, "ns_GetSegmentInfo");
  lib->GetSegmentSourceInfo = dlsym (dlh, "ns_GetSegmentSourceInfo");
  lib->GetSegmentData = dlsym (dlh, "ns_GetSegmentData");
  lib->GetNeuralInfo = dlsym (dlh, "ns_GetNeuralInfo");
  lib->GetNeuralData = dlsym (dlh, "ns_GetNeuralData");
  lib->GetIndexByTime = dlsym (dlh, "ns_GetIndexByTime");
  lib->GetTimeByIndex = dlsym (dlh, "ns_GetTimeByIndex");
  lib->GetLastErrorMsg = dlsym (dlh, "ns_GetLastErrorMsg");

  lib_handle = PyCObject_FromVoidPtr (lib, NULL);
  
  return lib_handle;
}

static PyObject *
library_close (PyObject *self, PyObject *args, PyObject *kwds)
{
  PyObject  *cobj;
  NsLibrary *lib;
  int        res;

  if (!PyArg_ParseTuple (args, "O", &cobj))
    {
      PyErr_SetString (PyExc_StandardError, "Could not parse arguments");
      return NULL;
    }

  if (!PyCObject_Check (cobj))
    {
      PyErr_SetString (PyExc_TypeError, "Expected NsLibrary type");
      return NULL;
    }
  
  lib = PyCObject_AsVoidPtr (cobj);
  
  res = dlclose (lib->dl_handle);
  
  if (res != 0)
    {
      PyErr_Format (PgError, "Could not load library: %s", dlerror ());
      return NULL;
    }

  free (lib);

  Py_RETURN_NONE;
}

static PyObject *
do_get_library_info (PyObject *self, PyObject *args, PyObject *kwds)
{
  PyObject       *cobj;
  PyObject       *dict;
  NsLibrary      *lib;
  ns_LIBRARYINFO  info;
  ns_RESULT       res;

  if (!PyArg_ParseTuple (args, "O", &cobj))
    {
      PyErr_SetString (PyExc_StandardError, "Could not parse arguments");
      return NULL;
    }
  
  if (!PyCObject_Check (cobj))
    {
      PyErr_SetString (PyExc_TypeError, "Expected NsLibrary type");
      return NULL;
    }
  
  lib = PyCObject_AsVoidPtr (cobj);
  res = lib->GetLibraryInfo (&info, sizeof (info));

  if (check_result_is_error (res, lib))
    return NULL;

  dict = PyDict_New ();
  dict_set_item_eat_ref (dict, "Description", PyString_FromString (info.szDescription));
  dict_set_item_eat_ref (dict, "Creator", PyString_FromString (info.szCreator));

  dict_set_item_eat_ref (dict, "LibVersionMaj", PyInt_FromLong (info.dwLibVersionMaj));
  dict_set_item_eat_ref (dict, "LibVersionMin", PyInt_FromLong (info.dwLibVersionMin));
  dict_set_item_eat_ref (dict, "APIVersionMaj", PyInt_FromLong (info.dwAPIVersionMaj));
  dict_set_item_eat_ref (dict, "APIVersionMin", PyInt_FromLong (info.dwAPIVersionMin));
  
  dict_set_item_eat_ref (dict, "Time_Year", PyInt_FromLong (info.dwTime_Year));
  dict_set_item_eat_ref (dict, "Time_Month", PyInt_FromLong (info.dwTime_Month));
  dict_set_item_eat_ref (dict, "Time_Day", PyInt_FromLong (info.dwTime_Day));

  dict_set_item_eat_ref (dict, "MaxFiles", PyInt_FromLong (info.dwMaxFiles));

  return dict;
}

static ns_RESULT
get_and_add_file_info (NsLibrary *lib,
		       uint32     file_id,
		       PyObject  *dict)
{
  ns_FILEINFO     info;
  ns_RESULT       res;

  res = lib->GetFileInfo (file_id, &info, sizeof (info));

  if (res != ns_OK)
    return res;

  dict_set_item_eat_ref (dict, "FileType", PyString_FromString (info.szFileType));
  dict_set_item_eat_ref (dict, "AppName", PyString_FromString (info.szAppName));
  dict_set_item_eat_ref (dict, "FileComment", PyString_FromString (info.szFileComment));

  dict_set_item_eat_ref (dict, "EntityCount", PyInt_FromLong (info.dwEntityCount));
  dict_set_item_eat_ref (dict, "TimeStampResolution", PyFloat_FromDouble (info.dTimeStampResolution));
  dict_set_item_eat_ref (dict, "TimeSpan", PyFloat_FromDouble (info.dTimeSpan));

  dict_set_item_eat_ref (dict, "Time_Year", PyInt_FromLong (info.dwTime_Year));
  dict_set_item_eat_ref (dict, "Time_Month", PyInt_FromLong (info.dwTime_Month));
  dict_set_item_eat_ref (dict, "Time_Day", PyInt_FromLong (info.dwTime_Day));
  dict_set_item_eat_ref (dict, "Time_Hour", PyInt_FromLong (info.dwTime_Hour));
  dict_set_item_eat_ref (dict, "Time_Min", PyInt_FromLong (info.dwTime_Min));
  dict_set_item_eat_ref (dict, "Time_Sec", PyInt_FromLong (info.dwTime_Sec));
  dict_set_item_eat_ref (dict, "Time_MilliSec", PyInt_FromLong (info.dwTime_MilliSec));

  return res;
}

static PyObject *
do_open_file (PyObject *self, PyObject *args, PyObject *kwds)
{
  const char     *filename;
  PyObject       *cobj;
  PyObject       *tuple;
  PyObject       *dict = NULL;
  NsLibrary      *lib;
  ns_RESULT       res;
  uint32          file_id;

  if (!PyArg_ParseTuple (args, "Os", &cobj, &filename))
    {
      PyErr_SetString (PyExc_StandardError, "Could not parse arguments");
      return NULL;
    }
  
  if (!PyCObject_Check (cobj)) 
    {
      PyErr_SetString (PyExc_TypeError, "Expected NsLibrary type");
      return NULL;
    }
  
  lib = PyCObject_AsVoidPtr (cobj);

  res = lib->OpenFile (filename, &file_id);

  if (res == ns_OK)
    {
      dict = PyDict_New ();
      res = get_and_add_file_info (lib, file_id, dict);
    }

  if (! check_result_is_error (res, lib))
    {
      tuple = PyTuple_New (2);
      PyTuple_SetItem (tuple, 0, PyInt_FromLong (file_id));
      PyTuple_SetItem (tuple, 1, dict);
    }
  else
    tuple = NULL;
    

  return tuple;
}

static PyObject *
do_close_file (PyObject *self, PyObject *args, PyObject *kwds)
{
  PyObject       *cobj;
  PyObject       *iobj;
  NsLibrary      *lib;
  ns_RESULT       res;
  uint32          file_id;

  if (!PyArg_ParseTuple (args, "OO", &cobj, &iobj))
    {
      PyErr_SetString (PyExc_StandardError, "Could not parse arguments");
      return NULL;
    }
  
  if (!PyCObject_Check (cobj) || !PyInt_Check (iobj)) 
    {
      PyErr_SetString (PyExc_TypeError, "Wrong argument type(s)");
      return NULL;
    }

  lib = PyCObject_AsVoidPtr (cobj);
  file_id = (uint32) PyInt_AsUnsignedLongMask (iobj);

  res = lib->CloseFile (file_id);

  if (check_result_is_error (res, lib))
      return NULL;

  Py_RETURN_NONE;
}

/* *********************************** */
/* entity infos */

static int
get_and_add_event_info (NsLibrary *lib,
			uint32     file_id,
			uint32     entity_id,
			PyObject  *dict)
{
  ns_EVENTINFO info;
  ns_RESULT    res;

  res = lib->GetEventInfo (file_id, entity_id, &info, sizeof (info));

  if (res != ns_OK)
    return res;

  dict_set_item_eat_ref (dict, "EventType", PyInt_FromLong (info.dwEventType));
  dict_set_item_eat_ref (dict, "MinDataLength", PyInt_FromLong (info.dwMinDataLength));
  dict_set_item_eat_ref (dict, "MaxDataLength", PyInt_FromLong (info.dwMaxDataLength));
  dict_set_item_eat_ref (dict, "CSVDesc", PyString_FromString (info.szCSVDesc));

  return ns_OK;
}

static int
get_and_add_analog_info (NsLibrary *lib,
			 uint32     file_id,
			 uint32     entity_id,
			 PyObject *dict)
{
  ns_ANALOGINFO info;
  ns_RESULT     res;

  res = lib->GetAnalogInfo (file_id, entity_id, &info, sizeof (info));
  if (res != ns_OK)
    return res;

  dict_set_item_eat_ref (dict, "SampleRate", PyFloat_FromDouble (info.dSampleRate));
  dict_set_item_eat_ref (dict, "MinVal", PyFloat_FromDouble (info.dMinVal));
  dict_set_item_eat_ref (dict, "MaxVal", PyFloat_FromDouble (info.dMaxVal));
  dict_set_item_eat_ref (dict, "zUnits", PyString_FromString (info.szUnits));

  dict_set_item_eat_ref (dict, "Resolution", PyFloat_FromDouble (info.dResolution));
  dict_set_item_eat_ref (dict, "LocationX", PyFloat_FromDouble (info.dLocationX));
  dict_set_item_eat_ref (dict, "LocationY", PyFloat_FromDouble (info.dLocationY));
  dict_set_item_eat_ref (dict, "LocationZ", PyFloat_FromDouble (info.dLocationZ));
  dict_set_item_eat_ref (dict, "LocationUser", PyFloat_FromDouble (info.dLocationUser));
  dict_set_item_eat_ref (dict, "HighFreqCorner", PyFloat_FromDouble (info.dHighFreqCorner));

  dict_set_item_eat_ref (dict, "HighFreqOrder", PyInt_FromLong (info.dwHighFreqOrder));
  dict_set_item_eat_ref (dict, "HighFilterType", PyString_FromString (info.szHighFilterType));
  dict_set_item_eat_ref (dict, "LowFreqCorner", PyFloat_FromDouble (info.dLowFreqCorner));
  dict_set_item_eat_ref (dict, "LowFreqOrder", PyInt_FromLong (info.dwLowFreqOrder));
  dict_set_item_eat_ref (dict, "LowFilterType", PyString_FromString (info.szLowFilterType));
  dict_set_item_eat_ref (dict, "ProbeInfo", PyString_FromString (info.szProbeInfo));

  return ns_OK;
}

static int
get_and_add_segment_source_info (NsLibrary *lib,
				 uint32     file_id,
				 uint32     entity_id,
				 uint32     source_id,
				 PyObject  *list)
{
  ns_SEGSOURCEINFO  info;
  ns_RESULT         res;
  PyObject         *dict;


  dict = PyDict_New ();
  PyList_SetItem (list, source_id, dict);
  
  res = lib->GetSegmentSourceInfo (file_id,
				   entity_id,
				   source_id,
				   &info,
				   sizeof (info));

  if (res != ns_OK)
    return res;
  
  dict_set_item_eat_ref (dict, "MinVal", PyFloat_FromDouble (info.dMinVal));
  dict_set_item_eat_ref (dict, "MaxVal", PyFloat_FromDouble (info.dMaxVal));
  dict_set_item_eat_ref (dict, "SubSampleShift", PyFloat_FromDouble (info.dSubSampleShift));
  dict_set_item_eat_ref (dict, "Resolution", PyFloat_FromDouble (info.dResolution));

  dict_set_item_eat_ref (dict, "LocationX", PyFloat_FromDouble (info.dLocationX));
  dict_set_item_eat_ref (dict, "LocationY", PyFloat_FromDouble (info.dLocationY));
  dict_set_item_eat_ref (dict, "LocationZ", PyFloat_FromDouble (info.dLocationZ));
  dict_set_item_eat_ref (dict, "LocationUser", PyFloat_FromDouble (info.dLocationUser));
  dict_set_item_eat_ref (dict, "HighFreqCorner", PyFloat_FromDouble (info.dHighFreqCorner));

  dict_set_item_eat_ref (dict, "HighFreqOrder", PyInt_FromLong (info.dwHighFreqOrder));
  dict_set_item_eat_ref (dict, "HighFilterType", PyString_FromString (info.szHighFilterType));
  dict_set_item_eat_ref (dict, "LowFreqCorner", PyFloat_FromDouble (info.dLowFreqCorner));
  dict_set_item_eat_ref (dict, "LowFreqOrder", PyInt_FromLong (info.dwLowFreqOrder));
  dict_set_item_eat_ref (dict, "LowFilterType", PyString_FromString (info.szLowFilterType));
  dict_set_item_eat_ref (dict, "ProbeInfo", PyString_FromString (info.szProbeInfo));


  return ns_OK;
}


static int
get_and_add_segment_info (NsLibrary *lib,
			  uint32     file_id,
			  uint32     entity_id,
			  PyObject  *dict)
{
  ns_SEGMENTINFO  info;
  ns_RESULT       res;
  PyObject       *list;
  int             i;

  res = lib->GetSegmentInfo (file_id, entity_id, &info, sizeof (info));

  if (res != ns_OK)
    return res;

  dict_set_item_eat_ref (dict, "SourceCount", PyInt_FromLong (info.dwSourceCount));
  dict_set_item_eat_ref (dict, "MinSampleCount", PyInt_FromLong (info.dwMinSampleCount));
  dict_set_item_eat_ref (dict, "MaxSampleCount", PyInt_FromLong (info.dwMaxSampleCount));
  dict_set_item_eat_ref (dict, "SampleRate", PyFloat_FromDouble (info.dSampleRate));
  dict_set_item_eat_ref (dict, "Units", PyString_FromString (info.szUnits));

  list = PyList_New (info.dwSourceCount);

  for (i = 0; i < info.dwSourceCount; i++)
    get_and_add_segment_source_info (lib, file_id, entity_id, i, list);
    

  dict_set_item_eat_ref (dict, "SourceInfos", list);

  return res;
}

static int
get_and_add_neural_info (NsLibrary *lib,
			 uint32     file_id,
			 uint32     entity_id,
			 PyObject *dict)
{
  ns_NEURALINFO info;
  ns_RESULT     res;

  res = lib->GetNeuralInfo (file_id, entity_id, &info, sizeof (info));

  if (res != ns_OK)
    return res;

  dict_set_item_eat_ref (dict, "SourceEntityID", PyInt_FromLong (info.dwSourceEntityID));
  dict_set_item_eat_ref (dict, "SourceUnitID", PyInt_FromLong (info.dwSourceUnitID));
  dict_set_item_eat_ref (dict, "ProbeInfo", PyString_FromString (info.szProbeInfo));

  return ns_OK;
}

/* ************************************************************************** */
/* "public" API */

static PyObject *
do_get_entity_info (PyObject *self, PyObject *args, PyObject *kwds)
{
  ns_ENTITYINFO   info;
  PyObject       *cobj;
  PyObject       *iobj, *id_obj;
  PyObject       *dict;
  NsLibrary      *lib;
  ns_RESULT       res;
  uint32          file_id;
  uint32          entity_id;

  if (!PyArg_ParseTuple (args, "OOO", &cobj, &iobj, &id_obj))
    {
      PyErr_SetString (PyExc_StandardError, "Could not parse arguments");
      return NULL;
    }
  
  if (!PyCObject_Check (cobj) || !PyInt_Check (iobj) ||
      !PyInt_Check (id_obj)) 
    {
      PyErr_SetString (PyExc_TypeError, "Wrong argument type(s)");
      return NULL;
    }

  lib = PyCObject_AsVoidPtr (cobj);
  file_id = (uint32) PyInt_AsUnsignedLongMask (iobj);
  entity_id = (uint32) PyInt_AsUnsignedLongMask (id_obj);

  res = lib->GetEntityInfo (file_id, entity_id, &info, sizeof (info));

  if (check_result_is_error (res, lib))
    return NULL;

  dict = PyDict_New ();
  dict_set_item_eat_ref (dict, "EntityLabel", PyString_FromString (info.szEntityLabel));
  dict_set_item_eat_ref (dict, "EntityType", PyInt_FromLong (info.dwEntityType));
  dict_set_item_eat_ref (dict, "ItemCount", PyInt_FromLong (info.dwItemCount));

  switch (info.dwEntityType)
    {
    case ns_ENTITY_EVENT:
      res = get_and_add_event_info (lib, file_id, entity_id, dict);
      break;

    case ns_ENTITY_ANALOG:
      res = get_and_add_analog_info (lib, file_id, entity_id, dict);
      break;

    case ns_ENTITY_SEGMENT:
      res = get_and_add_segment_info (lib, file_id, entity_id, dict);
      break;

    case ns_ENTITY_NEURALEVENT:
      res = get_and_add_neural_info (lib, file_id, entity_id, dict);
      break;
    }

  if (check_result_is_error (res, lib))
    {
      Py_DECREF (dict);
      return NULL;
    }

  return dict;
}

/* ************************************ */

static PyObject *
do_get_event_data (PyObject *self, PyObject *args, PyObject *kwds)
{
  NsLibrary      *lib;
  PyObject       *cobj;
  PyObject       *iobj, *id_obj, *idx_obj, *tp_obj, *sz_obj;
  PyObject       *data_obj, *res_obj;
  uint32          file_id;
  uint32          entity_id;
  uint32          index;
  uint32          event_type;
  uint32          data_size;
  uint32          data_ret_size;
  double          time_stamp;
  ns_RESULT       res;
  void           *buffer;


  if (!PyArg_ParseTuple (args, "OOOOOO", &cobj, &iobj, &id_obj, &idx_obj, &tp_obj, &sz_obj))
    {
      PyErr_SetString (PyExc_StandardError, "Could not parse arguments");
      return NULL;
    }
  
  if (!PyCObject_Check (cobj) || !PyInt_Check (iobj) ||
      !PyInt_Check (id_obj) || !PyInt_Check (idx_obj) ||
      !PyInt_Check (tp_obj) || !PyInt_Check (sz_obj))
    {
      PyErr_SetString (PyExc_TypeError, "Wrong argument type(s)");
      return NULL;
    }
  
  lib = PyCObject_AsVoidPtr (cobj);
  file_id = (uint32) PyInt_AsUnsignedLongMask (iobj);
  entity_id = (uint32) PyInt_AsUnsignedLongMask (id_obj);
  index = (uint32) PyInt_AsUnsignedLongMask (idx_obj);
  event_type = (uint32) PyInt_AsUnsignedLongMask (tp_obj);
  data_size = (uint32) PyInt_AsUnsignedLongMask (sz_obj);

  /* ** */
  buffer = malloc (data_size);

  res = lib->GetEventData (file_id,
			   entity_id,
			   index,
			   &time_stamp,
			   buffer,
			   data_size, 
			   &data_ret_size);
  
  if (check_result_is_error (res, lib))
    {
      free (buffer);
      return NULL;
    }
	   
  switch (event_type)
    {
    case ns_EVENT_TEXT:
    case ns_EVENT_CSV:       
      data_obj = PyString_FromStringAndSize (buffer, data_ret_size);
      break;
      
    case ns_EVENT_BYTE:
      data_obj = PyInt_FromLong (uint8_from_data (buffer, data_ret_size));
      break;
      
    case ns_EVENT_WORD:
      data_obj = PyInt_FromLong (uint16_from_data (buffer, data_ret_size));
      break;
      
    case ns_EVENT_DWORD:
      data_obj = PyInt_FromLong (uint32_from_data (buffer, data_ret_size));
      break;
      
    default:
      Py_INCREF (Py_None);
      data_obj = Py_None;
    }

  res_obj = Py_BuildValue ("(d,O)", time_stamp, data_obj);
  free (buffer);
  return res_obj;
}

static PyObject *
do_get_analog_data (PyObject *self, PyObject *args, PyObject *kwds)
{
  NsLibrary      *lib;
  PyObject       *cobj;
  PyObject       *iobj, *id_obj, *idx_obj, *sz_obj;
  PyObject       *res_obj;
  PyObject       *array;
  uint32          file_id;
  uint32          entity_id;
  uint32          index;
  uint32          count;
  uint32          cont_count;
  ns_RESULT       res;
  void           *buffer;
  npy_intp        dims[1];

  if (!PyArg_ParseTuple (args, "OOOOO", &cobj, &iobj, &id_obj, &idx_obj, &sz_obj))
    {
      PyErr_SetString (PyExc_StandardError, "Could not parse arguments");
      return NULL;
    }

  if (!PyCObject_Check (cobj) || !PyInt_Check (iobj) ||
      !PyInt_Check (id_obj) || !PyInt_Check (idx_obj) ||
      !PyInt_Check (sz_obj))
    {
      PyErr_SetString (PyExc_TypeError, "Wrong argument type(s)");
      return NULL;
    }

  lib = PyCObject_AsVoidPtr (cobj);
  file_id = (uint32) PyInt_AsUnsignedLongMask (iobj);
  entity_id = (uint32) PyInt_AsUnsignedLongMask (id_obj);
  index = (uint32) PyInt_AsUnsignedLongMask (idx_obj);
  count = (uint32)  PyInt_AsUnsignedLongMask (sz_obj);

  /* ** */
  dims[0] = count; //sample count

  array = PyArray_New (&PyArray_Type,
		       1,
		       dims,
		       NPY_DOUBLE,
		       NULL,
		       NULL /* data */,
		       0 /* itemsize */,
		       NPY_CARRAY,
		       NULL);
  
  buffer = PyArray_DATA (array);

  res = lib->GetAnalogData (file_id,
			    entity_id,
			    index,
			    count,
			    &cont_count,
			    buffer);

  if (check_result_is_error (res, lib))
    {
      Py_DECREF (array);
      return NULL;
    }

  res_obj = PyTuple_New (2);
  PyTuple_SetItem (res_obj, 0, array);
  PyTuple_SetItem (res_obj, 1, PyInt_FromLong (cont_count));

  return res_obj;
}

static PyObject *
do_get_segment_data (PyObject *self, PyObject *args, PyObject *kwds)
{
  NsLibrary      *lib;
  PyObject       *cobj;
  PyObject       *iobj, *id_obj, *idx_obj, *sz_obj, *src_obj;
  PyObject       *res_obj;
  PyObject       *array;
  uint32          file_id;
  uint32          entity_id;
  uint32          index;
  uint32          count;
  uint32          sources;
  uint32          sample_count;
  uint32          uint_id;
  uint32          buffer_size;
  ns_RESULT       res;
  double         *buffer;
  npy_intp        dims[2];
  double          time_stamp;

  if (!PyArg_ParseTuple (args, "OOOOOO", &cobj, &iobj, &id_obj, &idx_obj, &src_obj, &sz_obj))
    {
      PyErr_SetString (PyExc_StandardError, "Could not parse arguments");
      return NULL;
    }

  if (!PyCObject_Check (cobj) || !PyInt_Check (iobj) ||
      !PyInt_Check (id_obj) || !PyInt_Check (idx_obj) ||
      !PyInt_Check (sz_obj) || !PyInt_Check (src_obj))
    {
      PyErr_SetString (PyExc_TypeError, "Wrong argument type(s)");
      return NULL;
    }

  lib = PyCObject_AsVoidPtr (cobj);
  file_id = (uint32) PyInt_AsUnsignedLongMask (iobj);
  entity_id = (uint32) PyInt_AsUnsignedLongMask (id_obj);
  index = (uint32) PyInt_AsUnsignedLongMask (idx_obj);
  count = (uint32)  PyInt_AsUnsignedLongMask (sz_obj);
  sources = (uint32)  PyInt_AsUnsignedLongMask (src_obj);

  /* ** */
  dims[0] = sources; //source count
  dims[1] = count; //sample count

  array = PyArray_New (&PyArray_Type,
		       2,
		       dims,
		       NPY_DOUBLE,
		       NULL,
		       NULL /* data */,
		       0 /* itemsize */,
		       0,
		       NULL);
  
  buffer = (double *) PyArray_DATA (array);
  buffer_size = PyArray_NBYTES (array);

  res = lib->GetSegmentData (file_id,
			     entity_id,
			     index,
			     &time_stamp,
			     buffer,
			     buffer_size,
			     &sample_count,
			     &uint_id);

  if (check_result_is_error (res, lib))
    {
      Py_DECREF (array);
      return NULL;
    }
  
  res_obj = PyTuple_New (4);
  PyTuple_SetItem (res_obj, 0, array);
  PyTuple_SetItem (res_obj, 1, PyInt_FromLong (time_stamp));
  PyTuple_SetItem (res_obj, 2, PyInt_FromLong (sample_count));
  PyTuple_SetItem (res_obj, 3, PyInt_FromLong (uint_id));

  return res_obj;
}

static PyObject *
do_get_neural_data (PyObject *self, PyObject *args, PyObject *kwds)
{
  NsLibrary      *lib;
  PyObject       *cobj;
  PyObject       *iobj, *id_obj, *idx_obj, *sz_obj;
  PyObject       *array;
  uint32          file_id;
  uint32          entity_id;
  uint32          index;
  uint32          index_count;
  ns_RESULT       res;
  void           *buffer;
  npy_intp        dims[1];

  if (!PyArg_ParseTuple (args, "OOOOO", &cobj, &iobj, &id_obj, &idx_obj, &sz_obj))
    {
      PyErr_SetString (PyExc_StandardError, "Could not parse arguments");
      return NULL;
    }

  if (!PyCObject_Check (cobj) || !PyInt_Check (iobj) ||
      !PyInt_Check (id_obj) || !PyInt_Check (idx_obj) ||
      !PyInt_Check (sz_obj))
    {
      PyErr_SetString (PyExc_TypeError, "Wrong argument type(s)");
      return NULL;
    }

  lib = PyCObject_AsVoidPtr (cobj);
  file_id = (uint32) PyInt_AsUnsignedLongMask (iobj);
  entity_id = (uint32) PyInt_AsUnsignedLongMask (id_obj);
  index = (uint32) PyInt_AsUnsignedLongMask (idx_obj);
  index_count = (uint32)  PyInt_AsUnsignedLongMask (sz_obj);

  /* ** */
  dims[0] = index_count;
  array = PyArray_New (&PyArray_Type,
		       1,
		       dims,
		       NPY_DOUBLE,
		       NULL,
		       NULL /* data */,
		       0 /* itemsize */,
		       NPY_CARRAY,
		       NULL);
  
  buffer = PyArray_DATA (array);

  res = lib->GetNeuralData (file_id,
			    entity_id,
			    index,
			    index_count,
			    buffer);

  if (check_result_is_error (res, lib))
    {
      Py_DECREF (array);
      return NULL;
    }

  return array;
}

static PyObject *
do_get_index_by_time(PyObject *self, PyObject *args, PyObject *kwds)
{
  NsLibrary  *lib;
  PyObject   *cobj;
  PyObject   *iobj, *id_obj, *tp_obj, *fl_obj;
  double      timepoint;
  uint32      file_id;
  uint32      entity_id;
  uint32      index;
  uint32      flags;
  ns_RESULT   res;


  if (!PyArg_ParseTuple (args, "OOOOO", &cobj, &iobj, &id_obj, &tp_obj, &fl_obj))
    {
      PyErr_SetString (PyExc_StandardError, "Could not parse arguments");
      return NULL;
    }

  if (!PyCObject_Check (cobj) || !PyInt_Check (iobj) ||
      !PyInt_Check (id_obj) || !PyFloat_Check (tp_obj) ||
      !PyInt_Check (fl_obj))
    {
      PyErr_SetString (PyExc_TypeError, "Wrong argument type(s)");
      return NULL;
    }

  lib = PyCObject_AsVoidPtr (cobj);
  file_id = (uint32) PyInt_AsUnsignedLongMask (iobj);
  entity_id = (uint32) PyInt_AsUnsignedLongMask (id_obj);
  timepoint = PyFloat_AsDouble (tp_obj);
  flags = (uint32) PyInt_AsUnsignedLongMask (fl_obj);

  res = lib->GetIndexByTime (file_id,
			     entity_id,
			     timepoint,
			     flags,
			     &index);
  
  if (check_result_is_error (res, lib))
    return NULL;

  return PyInt_FromLong (index);
}

static PyObject *
do_get_time_by_index (PyObject *self, PyObject *args, PyObject *kwds)
{
  NsLibrary      *lib;
  PyObject       *cobj;
  PyObject       *iobj, *id_obj, *idx_obj;
  uint32          file_id;
  uint32          entity_id;
  uint32          index;
  double          timepoint;
  ns_RESULT       res;

  if (!PyArg_ParseTuple (args, "OOOO", &cobj, &iobj, &id_obj, &idx_obj))
    {
      PyErr_SetString (PyExc_StandardError, "Could not parse arguments");
      return NULL;
    }

  if (!PyCObject_Check (cobj) || !PyInt_Check (iobj) ||
      !PyInt_Check (id_obj) || !PyInt_Check (idx_obj))
    {
      PyErr_SetString (PyExc_TypeError, "Wrong argument type(s)");
      return NULL;
    }

  lib = PyCObject_AsVoidPtr (cobj);
  file_id = (uint32) PyInt_AsUnsignedLongMask (iobj);
  entity_id = (uint32) PyInt_AsUnsignedLongMask (id_obj);
  index = (uint32) PyInt_AsUnsignedLongMask (idx_obj);

  res = lib->GetTimeByIndex (file_id,
			     entity_id,
			     index,
			     &timepoint);
  
  if (check_result_is_error (res, lib))
    return NULL;

  return Py_BuildValue ("d", timepoint);
}


static PyMethodDef NativeMethods[] = {

  {"library_open", (PyCFunction) library_open, METH_VARARGS | METH_KEYWORDS,
   "Open a Neuroshare Library"},
  {"library_close",  (PyCFunction) library_close, METH_VARARGS | METH_KEYWORDS,
   "Close an open Neuroshare Library"},

  {"_get_library_info",  (PyCFunction) do_get_library_info, METH_VARARGS | METH_KEYWORDS,
   "Retrieves information about the loaded API library"},
  {"_open_file",  (PyCFunction) do_open_file, METH_VARARGS | METH_KEYWORDS,
   "Opens the data file and returns its file info."},
  {"_close_file",  (PyCFunction) do_close_file, METH_VARARGS | METH_KEYWORDS,
   "Close the open data file"},
  {"_get_entity_info",  (PyCFunction) do_get_entity_info, METH_VARARGS | METH_KEYWORDS,
   "Retrieve Entity (general and specific) information"},

  {"_get_event_data",  (PyCFunction) do_get_event_data, METH_VARARGS | METH_KEYWORDS,
   "Retrieve event data"},
  {"_get_analog_data",  (PyCFunction) do_get_analog_data, METH_VARARGS | METH_KEYWORDS,
   "Retrieve analog data"},
  {"_get_segment_data",  (PyCFunction) do_get_segment_data, METH_VARARGS | METH_KEYWORDS,
   "Retrieve segment data"},
  {"_get_neural_data",  (PyCFunction) do_get_neural_data, METH_VARARGS | METH_KEYWORDS,
   "Retrieve analog data"},

  {"_get_time_by_index",  (PyCFunction) do_get_time_by_index, METH_VARARGS | METH_KEYWORDS,
   "Timestamp of the index"},
  {"_get_index_by_time",  (PyCFunction) do_get_index_by_time, METH_VARARGS | METH_KEYWORDS,
   "Index by timepoint"},



  {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC
init_capi(void)
{
  PyObject *module;
  
  module = Py_InitModule ("neuroshare._capi", NativeMethods);
  if (module == NULL)
    return;
  
  PyModule_AddStringConstant (module,
			      "__doc__",
			      "neuroshare native (C) functions");
  
  import_array ();
  
  PgError = PyErr_NewException ("_capi.error", NULL, NULL);
  Py_INCREF (PgError);
  PyModule_AddObject (module, "error", PgError);
}