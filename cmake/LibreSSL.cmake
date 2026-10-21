find_package(Git REQUIRED)
get_filename_component(LIBRESSL_GIT_DIR ${GIT_EXECUTABLE} DIRECTORY)
find_program(PERL_EXECUTABLE NAMES perl HINTS ${LIBRESSL_GIT_DIR}/../usr/bin)
find_package(Perl REQUIRED)

set(LIBRESSL_PORTABLE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/libressl-portable)
set(LIBRESSL_OPENBSD_DIR ${CMAKE_SOURCE_DIR}/3rdparty/libressl-openbsd/src)
set(LIBRESSL_PREPARED_DIR ${CMAKE_BINARY_DIR}/3rdparty/libressl)

function(libressl_copy_stripped source destination)
    file(READ ${source} content)
    string(REGEX REPLACE "[^\n]*DEF_WEAK[^\n]*\n" "" content "${content}")
    get_filename_component(name ${source} NAME)
    file(WRITE ${destination}/${name} "${content}")
endfunction()

function(libressl_makefile_entries makefiles filter output)
    set(entries)
    foreach(makefile ${makefiles})
        file(STRINGS ${makefile} lines REGEX "${filter}")
        foreach(line ${lines})
            string(REGEX REPLACE "^[ \t]*" "" line "${line}")
            string(REGEX REPLACE "[ \t]+" ";" fields "${line}")
            list(LENGTH fields count)
            if(count GREATER 2)
                list(GET fields 2 entry)
                if(NOT entry MATCHES "^\\$\\(")
                    list(APPEND entries ${entry})
                endif()
            endif()
        endforeach()
    endforeach()
    list(REMOVE_DUPLICATES entries)
    set(${output} ${entries} PARENT_SCOPE)
endfunction()

function(libressl_write_version shlib destination)
    file(STRINGS ${shlib} lines)
    foreach(line ${lines})
        if(line MATCHES "^(major|minor)=([0-9]+)")
            set(${CMAKE_MATCH_1} ${CMAKE_MATCH_2})
        endif()
    endforeach()
    file(WRITE ${destination} "${major}:${minor}:0\n")
endfunction()

function(libressl_write_symbols list destination)
    file(STRINGS ${list} lines REGEX "^[A-Za-z0-9_]")
    list(JOIN lines "\n" content)
    file(WRITE ${destination} "${content}\n")
endfunction()

function(libressl_prepare)
    set(portable ${LIBRESSL_PORTABLE_DIR})
    set(openbsd ${LIBRESSL_OPENBSD_DIR})
    set(out ${LIBRESSL_PREPARED_DIR})
    set(libc ${openbsd}/lib/libc)
    set(libcrypto ${openbsd}/lib/libcrypto)
    set(libssl ${openbsd}/lib/libssl)
    set(libtls ${openbsd}/lib/libtls)

    execute_process(COMMAND ${GIT_EXECUTABLE} -C ${portable} rev-parse HEAD OUTPUT_VARIABLE portableHead OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${GIT_EXECUTABLE} -C ${openbsd} rev-parse HEAD OUTPUT_VARIABLE openbsdHead OUTPUT_STRIP_TRAILING_WHITESPACE)
    file(READ ${CMAKE_CURRENT_FUNCTION_LIST_FILE} script)
    string(SHA256 scriptHash "${script}")
    set(stamp "${portableHead} ${openbsdHead} ${scriptHash}")
    if(EXISTS ${out}/.anyps5-stamp)
        file(READ ${out}/.anyps5-stamp previous)
        if(previous STREQUAL stamp)
            return()
        endif()
    endif()

    message(STATUS "Preparing LibreSSL sources in ${out}")
    file(REMOVE_RECURSE ${out})
    file(COPY ${portable}/ DESTINATION ${out} PATTERN ".git" EXCLUDE)

    libressl_write_version(${libcrypto}/shlib_version ${out}/crypto/VERSION)
    libressl_write_version(${libssl}/shlib_version ${out}/ssl/VERSION)
    libressl_write_version(${libtls}/shlib_version ${out}/tls/VERSION)

    file(MAKE_DIRECTORY ${out}/include/openssl ${out}/crypto/objects)
    file(COPY ${libcrypto}/opensslconf.h ${libcrypto}/opensslfeatures.h DESTINATION ${out}/include/openssl)
    file(COPY ${libssl}/pqueue.h ${libtls}/tls.h DESTINATION ${out}/include)

    file(GLOB entropy ${libcrypto}/arc4random/getentropy_*.c ${libcrypto}/arc4random/arc4random_*.h)
    foreach(source
            ${libc}/crypt/arc4random.c ${libc}/crypt/arc4random_uniform.c ${libc}/crypt/chacha_private.h
            ${libc}/stdlib/reallocarray.c ${libc}/stdlib/recallocarray.c ${libc}/stdlib/strtonum.c
            ${libc}/string/explicit_bzero.c ${libc}/string/strcasecmp.c ${libc}/string/strlcpy.c
            ${libc}/string/strlcat.c ${libc}/string/strndup.c ${libc}/string/strnlen.c ${libc}/string/strsep.c
            ${libc}/string/timingsafe_bcmp.c ${libc}/string/timingsafe_memcmp.c ${entropy})
        libressl_copy_stripped(${source} ${out}/crypto/compat)
    endforeach()

    execute_process(
        COMMAND ${PERL_EXECUTABLE} objects.pl objects.txt obj_mac.num ${out}/include/openssl/obj_mac.h
        WORKING_DIRECTORY ${libcrypto}/objects
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "LibreSSL objects.pl failed: ${result}")
    endif()
    execute_process(
        COMMAND ${PERL_EXECUTABLE} obj_dat.pl ${out}/include/openssl/obj_mac.h ${out}/crypto/objects/obj_dat.h
        WORKING_DIRECTORY ${libcrypto}/objects
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "LibreSSL obj_dat.pl failed: ${result}")
    endif()

    foreach(header
            stack/stack.h lhash/lhash.h stack/safestack.h ossl_typ.h err/err.h crypto.h comp/comp.h x509/x509.h
            buffer/buffer.h objects/objects.h asn1/asn1.h asn1/posix_time.h bn/bn.h ec/ec.h ecdsa/ecdsa.h
            ecdh/ecdh.h rsa/rsa.h sha/sha.h x509/x509_vfy.h pkcs7/pkcs7.h pem/pem.h hkdf/hkdf.h hmac/hmac.h
            rand/rand.h md5/md5.h mlkem/mlkem.h x509/x509v3.h conf/conf.h ocsp/ocsp.h aes/aes.h modes/modes.h
            asn1/asn1t.h bf/blowfish.h bio/bio.h cast/cast.h cmac/cmac.h cms/cms.h des/des.h dh/dh.h dsa/dsa.h
            engine/engine.h ui/ui.h pkcs12/pkcs12.h ts/ts.h md4/md4.h ripemd/ripemd.h idea/idea.h rc2/rc2.h
            rc4/rc4.h txt_db/txt_db.h sm3/sm3.h sm4/sm4.h chacha/chacha.h evp/evp.h poly1305/poly1305.h
            camellia/camellia.h curve25519/curve25519.h ct/ct.h kdf/kdf.h)
        file(COPY ${libcrypto}/${header} DESTINATION ${out}/include/openssl)
    endforeach()
    foreach(header srtp.h ssl.h ssl3.h tls1.h dtls1.h)
        file(COPY ${libssl}/${header} DESTINATION ${out}/include/openssl)
    endforeach()
    file(COPY ${portable}/patches/opensslv.h DESTINATION ${out}/include/openssl)
    file(STRINGS ${out}/include/openssl/opensslv.h versionLine REGEX "LIBRESSL_VERSION_TEXT" LIMIT_COUNT 1)
    string(REGEX MATCH "LibreSSL ([0-9.]+)" unused "${versionLine}")
    file(WRITE ${out}/VERSION "${CMAKE_MATCH_1}\n")

    file(GLOB cryptoMakefiles ${portable}/crypto/Makefile.am*)
    libressl_makefile_entries("${cryptoMakefiles}" "^ASM|SOURCES|HEADERS" cryptoFiles)
    file(TOUCH ${out}/crypto/empty.c)
    foreach(file ${cryptoFiles})
        get_filename_component(dir ${file} DIRECTORY)
        if(NOT dir STREQUAL "compat" AND EXISTS ${libcrypto}/${file})
            get_filename_component(target ${out}/crypto/${file} DIRECTORY)
            file(COPY ${libcrypto}/${file} DESTINATION ${target})
        endif()
    endforeach()
    file(COPY ${out}/crypto/compat/b_win.c DESTINATION ${out}/crypto/bio)
    file(COPY ${out}/crypto/compat/ui_openssl_win.c DESTINATION ${out}/crypto/ui)
    libressl_write_symbols(${libcrypto}/Symbols.list ${out}/crypto/crypto.sym)

    libressl_makefile_entries(${portable}/tls/Makefile.am "SOURCES|HEADERS" tlsFiles)
    file(TOUCH ${out}/tls/empty.c)
    foreach(file ${tlsFiles})
        if(EXISTS ${libtls}/${file})
            file(COPY ${libtls}/${file} DESTINATION ${out}/tls)
        endif()
    endforeach()
    libressl_write_symbols(${libtls}/Symbols.list ${out}/tls/tls.sym)

    libressl_makefile_entries(${portable}/ssl/Makefile.am "SOURCES|HEADERS" sslFiles)
    file(TOUCH ${out}/ssl/empty.c)
    foreach(file ${sslFiles})
        if(NOT file MATCHES "crypto_arch" AND EXISTS ${libssl}/${file})
            get_filename_component(target ${out}/ssl/${file} DIRECTORY)
            file(COPY ${libssl}/${file} DESTINATION ${target})
        endif()
    endforeach()
    libressl_write_symbols(${libssl}/Symbols.list ${out}/ssl/ssl.sym)

    file(GLOB patches ${portable}/patches/*.patch)
    get_filename_component(ceiling ${out} DIRECTORY)
    foreach(patch ${patches})
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E env GIT_CEILING_DIRECTORIES=${ceiling}
                ${GIT_EXECUTABLE} apply -p0 --exclude=apps/* --exclude=tests/* ${patch}
            WORKING_DIRECTORY ${out}
            RESULT_VARIABLE result
            ERROR_VARIABLE error)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "LibreSSL patch ${patch} failed: ${error}")
        endif()
    endforeach()

    file(WRITE ${out}/.anyps5-stamp "${stamp}")
endfunction()

libressl_prepare()

set(LIBRESSL_APPS OFF CACHE BOOL "" FORCE)
set(LIBRESSL_TESTS OFF CACHE BOOL "" FORCE)
set(LIBRESSL_SKIP_INSTALL ON CACHE BOOL "" FORCE)
set(ENABLE_ASM OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS_SAVED ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${LIBRESSL_PREPARED_DIR} ${CMAKE_BINARY_DIR}/3rdparty/libressl-build EXCLUDE_FROM_ALL)
set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_SAVED})
set_target_properties(crypto_obj compat_obj ssl_obj tls_obj tls_compat_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MINGW)
    foreach(target crypto_obj compat_obj ssl_obj tls_obj tls_compat_obj)
        target_compile_options(${target} PRIVATE -fno-asynchronous-unwind-tables)
    endforeach()
endif()
