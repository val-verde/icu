/*
 *******************************************************************************
 * Copyright (C) 2003, International Business Machines Corporation and         *
 * others. All Rights Reserved.                                                *
 *******************************************************************************
 */

#if !UCONFIG_NO_COLLATION

#include "svccoll.h"
#include "unicode/coll.h"
#include "unicode/strenum.h"
#include "hash.h"

void CollationServiceTest::TestRegister() 
{
  // register a singleton
  const Locale& FR = Locale::getFrance();
  const Locale& US = Locale::getUS();
  const Locale US_FOO("en", "US", "FOO");

  UErrorCode status = U_ZERO_ERROR;

  Collator* frcol = Collator::createInstance(FR, status);
  Collator* uscol = Collator::createInstance(US, status);

  { // try override en_US collator
    URegistryKey key = Collator::registerInstance(frcol, US, status);

    Collator* ncol = Collator::createInstance(US_FOO, status);
    if (*frcol != *ncol) {
      errln("register of french collator for en_US failed on request for en_US_FOO");
    }
	// ensure original collator's params not touched
	Locale loc = frcol->getLocale(ULOC_REQUESTED_LOCALE, status);
	if (loc != FR) {
		errln(UnicodeString("fr collator's requested locale changed to ") + loc.getName());
	}
	loc = frcol->getLocale(ULOC_VALID_LOCALE, status);
	if (loc != FR) {
		errln(UnicodeString("fr collator's valid locale changed to ") + loc.getName());
	}

	loc = ncol->getLocale(ULOC_REQUESTED_LOCALE, status);
	if (loc != US_FOO) {
		errln(UnicodeString("requested locale for en_US_FOO is not en_US_FOO but ") + loc.getName());
	}
	loc = ncol->getLocale(ULOC_VALID_LOCALE, status);
	if (loc != US) {
		errln(UnicodeString("valid locale for en_US_FOO is not en_US but ") + loc.getName());
	}
	loc = ncol->getLocale(ULOC_ACTUAL_LOCALE, status);
	if (loc != US) {
		errln(UnicodeString("actual locale for en_US_FOO is not en_US but ") + loc.getName());
	}
    delete ncol; ncol = NULL;

    if (!Collator::unregister(key, status)) {
      errln("failed to unregister french collator");
    }
    // !!! frcol pointer is now invalid !!!

    ncol = Collator::createInstance(US, status);
    if (*uscol != *ncol) {
      errln("collator after unregister does not match original");
    }
    delete ncol; ncol = NULL;
  }

  // recreate frcol
  frcol = Collator::createInstance(FR, status);

  { // try create collator for new locale
      Locale fu_FU("fu", "FU", "FOO");

    Collator* fucol = Collator::createInstance(fu_FU, status);
    URegistryKey key = Collator::registerInstance(frcol, fu_FU, status);
    Collator* ncol = Collator::createInstance(fu_FU, status);
    if (*frcol != *ncol) {
      errln("register of fr collator for fu_FU failed");
    }
    delete ncol; ncol = NULL;

    UnicodeString locName = fu_FU.getName();
    StringEnumeration* localeEnum = Collator::getAvailableLocales();
    UBool found = FALSE;
    for (const UnicodeString* loc = localeEnum->snext(status);
         !found && loc != NULL;
         loc = localeEnum->snext(status)) {
      //
      if (locName == *loc) {
        found = TRUE;
      }
    }
    delete localeEnum;
    
    if (!found) {
      errln("new locale fu_FU not reported as supported locale");
    }
            
    UnicodeString displayName;
    Collator::getDisplayName(fu_FU, displayName);
    if (displayName != "fu (FU, FOO)") {
      errln(UnicodeString("found ") + displayName + " for fu_FU");
    }

    Collator::getDisplayName(fu_FU, fu_FU, displayName);
    if (displayName != "fu (FU, FOO)") {
      errln(UnicodeString("found ") + displayName + " for fu_FU");
    }

    if (!Collator::unregister(key, status)) {
      errln("failed to unregister french collator");
    }
    // !!! note frcoll invalid again, but we're no longer using it
 
    ncol = Collator::createInstance(fu_FU, status);
    if (*fucol != *ncol) {
      errln("collator after unregister does not match original fu_FU");
    }
    delete ncol; ncol = NULL;
    delete fucol; fucol = NULL;
  }
}

// ------------------

struct CollatorInfo {
  Locale locale;
  Collator* collator;
  Hashtable* displayNames; // locale name -> string

  CollatorInfo(const Locale& locale, Collator* collatorToAdopt, Hashtable* displayNamesToAdopt);
  ~CollatorInfo();
  UnicodeString& getDisplayName(const Locale& displayLocale, UnicodeString& name) const;
};

CollatorInfo::CollatorInfo(const Locale& _locale, Collator* _collator, Hashtable* _displayNames)
  : locale(_locale)
  , collator(_collator)
  , displayNames(_displayNames)
{
}

CollatorInfo::~CollatorInfo() {
  delete collator;
  delete displayNames;
}

UnicodeString& 
CollatorInfo::getDisplayName(const Locale& displayLocale, UnicodeString& name) const {
  if (displayNames) {
    UnicodeString* val = (UnicodeString*)displayNames->get(displayLocale.getName());
    if (val) {
      name = *val;
      return name;
    }
  }

  return locale.getDisplayName(displayLocale, name);
}

// ---------------

class TestFactory : public CollatorFactory {
  CollatorInfo** info;
  int32_t count;
  UnicodeString* ids;

  const CollatorInfo* getInfo(const Locale& loc) const {
    for (CollatorInfo** p = info; *p; ++p) {
      if (loc == (**p).locale) {
        return *p;
      }
    }
    return NULL;
  }

public:       
  TestFactory(CollatorInfo** _info) 
    : info(_info)
    , count(0)
    , ids(NULL)
  {
    CollatorInfo** p;
    for (p = info; *p; ++p) {}
    count = p - info;
  }

  ~TestFactory() {
    for (CollatorInfo** p = info; *p; ++p) {
      delete *p;
    }
    delete info;
    delete[] ids;
  }

  virtual Collator* createCollator(const Locale& loc) {
    const CollatorInfo* ci = getInfo(loc);
    if (ci) {
      return ci->collator->clone();
    }
    return NULL;
  }

  virtual UnicodeString& getDisplayName(const Locale& objectLocale, 
                                        const Locale& displayLocale,
                                        UnicodeString& result)
  {
    const CollatorInfo* ci = getInfo(objectLocale);
    if (ci) {
      ci->getDisplayName(displayLocale, result);
    } else {
      result.setToBogus();
    }
    return result;
  }

  const UnicodeString* const getSupportedIDs(int32_t& _count, UErrorCode& status) {
    if (U_SUCCESS(status)) {
      if (!ids) {
        ids = new UnicodeString[count];
        if (!ids) {
          status = U_MEMORY_ALLOCATION_ERROR;
          _count = 0;
          return NULL;
        }

        for (int i = 0; i < count; ++i) {
          ids[i] = info[i]->locale.getName();
        }
      }

      _count = count;
      return ids;
    }
    return NULL;
  }

  virtual inline UClassID getDynamicClassID() const {
    return (UClassID)&gClassID;
  }

  static UClassID getStaticClassID() {
    return (UClassID)&gClassID;
  }

private:
  static char gClassID;
};

char TestFactory::gClassID = 0;

void CollationServiceTest::TestRegisterFactory(void) 
{
  Locale fu_FU("fu", "FU", "");
  Locale fu_FU_FOO("fu", "FU", "FOO");

  UErrorCode status = U_ZERO_ERROR;

  Hashtable* fuFUNames = new Hashtable(FALSE, status);
  if (!fuFUNames) {
    errln("memory allocation error");
    return;
  }
  fuFUNames->setValueDeleter(uhash_deleteUnicodeString);

  fuFUNames->put(fu_FU.getName(), new UnicodeString("ze leetle bunny Fu-Fu"), status);
  fuFUNames->put(fu_FU_FOO.getName(), new UnicodeString("zee leetel bunny Foo-Foo"), status);
  fuFUNames->put(Locale::getUS().getName(), new UnicodeString("little bunny Foo Foo"), status);

  Collator* frcol = Collator::createInstance(Locale::getFrance(), status);
  Collator* gecol = Collator::createInstance(Locale::getGermany(), status);
  Collator* jpcol = Collator::createInstance(Locale::getJapan(), status);

  CollatorInfo** info = new CollatorInfo*[4];
  if (!info) {
    errln("memory allocation error");
    return;
  }

  info[0] = new CollatorInfo(Locale::getUS(), frcol, NULL);
  info[1] = new CollatorInfo(Locale::getFrance(), gecol, NULL);
  info[2] = new CollatorInfo(fu_FU, jpcol, fuFUNames);
  info[3] = NULL;

  TestFactory* factory = new TestFactory(info);
  if (!factory) {
    errln("memory allocation error");
    return;
  }

  Collator* uscol = Collator::createInstance(Locale::getUS(), status);
  Collator* fucol = Collator::createInstance(fu_FU, status);
        
  {
    URegistryKey key = Collator::registerFactory(factory, status);
    Collator* ncol = Collator::createInstance(Locale::getUS(), status);
    if (*frcol != *ncol) {
      errln("frcoll for en_US failed");
    }
    delete ncol; ncol = NULL;

    ncol = Collator::createInstance(fu_FU_FOO, status);
    if (*jpcol != *ncol) {
      errln("jpcol for fu_FU_FOO failed");
    }

	Locale loc = ncol->getLocale(ULOC_REQUESTED_LOCALE, status);
	if (loc != fu_FU_FOO) {
		errln(UnicodeString("requested locale for fu_FU_FOO is not fu_FU_FOO but ") + loc.getName());
	}
	loc = ncol->getLocale(ULOC_VALID_LOCALE, status);
	if (loc != fu_FU) {
		errln(UnicodeString("valid locale for fu_FU_FOO is not fu_FU but ") + loc.getName());
	}
    delete ncol; ncol = NULL;

    UnicodeString locName = fu_FU.getName();
    StringEnumeration* localeEnum = Collator::getAvailableLocales();
    UBool found = FALSE;
    for (const UnicodeString* loc = localeEnum->snext(status);
         !found && loc != NULL;
         loc = localeEnum->snext(status)) {
      //
      if (locName == *loc) {
        found = TRUE;
      }
    }
    delete localeEnum;
    
    if (!found) {
      errln("new locale fu_FU not reported as supported locale");
    }

    UnicodeString name;
    Collator::getDisplayName(fu_FU, name);
    if (name != "little bunny Foo Foo") {
      errln(UnicodeString("found ") + name + " for fu_FU");
    }

    Collator::getDisplayName(fu_FU, fu_FU_FOO, name);
    if (name != "zee leetel bunny Foo-Foo") {
      errln(UnicodeString("found ") + name + " for fu_FU in fu_FU_FOO");
    }

    if (!Collator::unregister(key, status)) {
      errln("failed to unregister factory");
    }
    // ja, fr, ge collators no longer valid

    ncol = Collator::createInstance(fu_FU, status);
    if (*fucol != *ncol) {
      errln("collator after unregister does not match original fu_FU");
    }
  }

  delete fucol;
  delete uscol;
}

void CollationServiceTest::runIndexedTest(int32_t index, UBool exec, const char* &name, char* /*par */)
{
  if (exec) logln("TestSuite CollationServiceTest: ");
  switch (index) {
    TESTCASE(0, TestRegister);
    TESTCASE(1, TestRegisterFactory);
  default: name = ""; break;
  }
}

#endif
