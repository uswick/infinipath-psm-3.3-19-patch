# Copyright (c) 2012. Intel Corporation. All rights reserved.
# Copyright (c) 2010. QLogic Corporation. All rights reserved.
#
# This software is available to you under a choice of one of two
# licenses.  You may choose to be licensed under the terms of the GNU
# General Public License (GPL) Version 2, available from the file
# COPYING in the main directory of this source tree, or the
# OpenIB.org BSD license below:
#
#     Redistribution and use in source and binary forms, with or
#     without modification, are permitted provided that the following
#     conditions are met:
#
#      - Redistributions of source code must retain the above
#        copyright notice, this list of conditions and the following
#        disclaimer.
#
#      - Redistributions in binary form must reproduce the above
#        copyright notice, this list of conditions and the following
#        disclaimer in the documentation and/or other materials
#        provided with the distribution.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
# BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

Summary: Intel PSM Libraries
Name: infinipath-psm
Version: 3.3
Release: 19_g67c0807_open
Epoch: 4
License: GPL
Group: System Environment/Libraries
URL: http://www.intel.com/
Source0: %{name}-%{version}-%{release}.tar.gz
Prefix: /usr
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
Provides: infinipath-psm = %{version}
%if "%{PSM_HAVE_SCIF}" == "1"
Provides: intel-mic-psm = %{version}
%endif
# MIC package
Obsoletes: intel-mic-psm
# OFED package
Obsoletes: infinipath-libs <= %{version}-%{release}
Conflicts: infinipath-libs <= %{version}-%{release}
# mpss package
Obsoletes: mpss-psm <= %{version}-%{release}
Conflicts: mpss-psm <= %{version}-%{release}
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%package -n infinipath-psm-devel
Summary: Development files for Intel PSM
Group: System Environment/Development
Requires: infinipath-psm = %{version}-%{release}
Provides: infinipath-psm-devel = %{version}
%if "%{PSM_HAVE_SCIF}" == "1"
Provides: intel-mic-psm-devel = %{version}
%endif
# MIC package
Obsoletes: intel-mic-psm-devel
# OFED package
Obsoletes: infinipath-devel <= %{version}-%{release}
Conflicts: infinipath-devel <= %{version}-%{release}
# mpss package
Obsoletes: mpss-psm-dev <= %{version}-%{release}
Conflicts: mpss-psm-dev <= %{version}-%{release}
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

# %package card-devel
# Summary: Development files for Intel Xeon Phi
# Group: System Environment/Development
# Requires: %{name} = %{version}-%{release}
# Requires(post): /sbin/ldconfig
# Requires(postun): /sbin/ldconfig


%global debug_package %{nil}

#PSM_HAVE_SCIF is one of: 0 1
%{!?PSM_HAVE_SCIF:     %global PSM_HAVE_SCIF 0}

%define INFINIPATH_MAKEARG PSM_HAVE_SCIF=0 MIC=0
%define INTEL_MAKEARG PSM_HAVE_SCIF=1 MIC=0
%define INTEL_CARD_MAKEARG PSM_HAVE_SCIF=1 MIC=1 LOCAL_PREFIX=/opt/intel/mic/psm
%define card_prefix /opt/intel/mic/psm

%if "%{PSM_HAVE_SCIF}" == "0"
  %define MAKEARG PSM_HAVE_SCIF=0 MIC=0
%else
  %if "%{PSM_HAVE_SCIF}" == "1"
    %define MAKEARG PSM_HAVE_SCIF=1 MIC=0
  %else
    %define MAKEARG PSM_HAVE_SCIF=0 MIC=0
    %define PSM_HAVE_SCIF "1"
  %endif
%endif

%description
The PSM Messaging API, or PSM API, is Intel's low-level
user-level communications interface for the True Scale
family of products. PSM users are enabled with mechanisms
necessary to implement higher level communications
interfaces in parallel environments.

%description -n infinipath-psm-devel
Development files for the libpsm_infinipath library

%prep
%setup -q -n %{name}-%{version}-%{release}

%build
%{__make} USE_PSM_UUID=1 %{MAKEARG}

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT
export DESTDIR=$RPM_BUILD_ROOT
%{__make} install %{MAKEARG}

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig
%post devel -p /sbin/ldconfig
%postun devel -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
/usr/lib64/libpsm_infinipath.so.*
/usr/lib64/libinfinipath.so.*
%if "%{PSM_HAVE_SCIF}" == "1"
/usr/sbin/psmd
%endif

%files -n infinipath-psm-devel
%defattr(-,root,root,-)
/usr/lib64/libpsm_infinipath.so
/usr/lib64/libinfinipath.so
/usr/include/psm.h
/usr/include/psm_mq.h



%changelog
* Fri Sep 25 2015 Henry Estela <henry.r.estela@intel.com> - 3.3-1
- Always build infinipath-psm with different Provides names.
* Tue Nov 6 2012 Mitko Haralanov <mitko.haralanov@intel.com> - 3.3-1
- Add Intel Xeon Phi related changes
* Tue May 11 2010 Mitko Haralanov <mitko@qlogic.com> - 3.3-1
- Initial build.

