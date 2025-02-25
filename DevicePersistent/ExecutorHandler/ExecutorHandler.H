/*---------------------------------------------------------------------------*\
License
    This file is part of OGL.

    OGL is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OGL is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OGL.  If not, see <http://www.gnu.org/licenses/>.

Class
    Foam::ExecutorHandler

Author: Gregor Olenik <go@hpsim.de>

SourceFiles
    ExecutorHandler.C

\*---------------------------------------------------------------------------*/

#ifndef OGL_ExecutorHandler_INCLUDED_H
#define OGL_ExecutorHandler_INCLUDED_H

#include "DevicePersistent/Base/Base.H"
#include "fvCFD.H"

#include <ginkgo/ginkgo.hpp>

namespace Foam {

struct ExecutorInitFunctor {
    mutable std::shared_ptr<gko::experimental::mpi::communicator> comm_;

    const label device_id_;

    const word executor_name_;

    const word field_name_;

    const label verbose_;

    ExecutorInitFunctor(const word executor_name, const word field_name,
                        const label verbose, const label gpus_per_rank,
                        const bool force_host_buffer = false)
        : comm_((Pstream::parRun())  // TODO make this DRY
                    ? std::make_shared<gko::experimental::mpi::communicator>(
                          MPI_COMM_WORLD, force_host_buffer)
                    : NULL),
          device_id_((Pstream::parRun()) ? comm_->rank() / gpus_per_rank : 0),
          executor_name_(executor_name),
          field_name_(field_name),
          verbose_(verbose)
    {}


    void update(std::shared_ptr<gko::Executor>) const {}

    const std::string not_compiled_tag = "not compiled";
    const gko::version_info version = gko::version_info::get();
    std::shared_ptr<gko::Executor> init() const
    {
        auto host_exec = gko::share(gko::ReferenceExecutor::create());

        if (executor_name_ == "cuda") {
            if (version.cuda_version.tag == not_compiled_tag) {
                FatalErrorInFunction
                    << "CUDA Backend was not compiled. Recompile OGL/Ginkgo "
                       "with CUDA backend enabled."
                    << abort(FatalError);
            };

            return gko::share(gko::CudaExecutor::create(
                device_id_ % gko::CudaExecutor::get_num_devices(), host_exec));
        }
        if (executor_name_ == "dpcpp") {
            if (version.dpcpp_version.tag == not_compiled_tag) {
                FatalErrorInFunction
                    << "SYCL Backend was not compiled. Recompile OGL/Ginkgo "
                       "with SYCL backend enabled."
                    << abort(FatalError);
            };
            return gko::share(gko::DpcppExecutor::create(
                device_id_ % gko::DpcppExecutor::get_num_devices("gpu"),
                host_exec));
        }
        if (executor_name_ == "hip") {
            if (version.hip_version.tag == not_compiled_tag) {
                FatalErrorInFunction
                    << "HIP Backend was not compiled. Recompile OGL/Ginkgo "
                       "with HIP backend enabled."
                    << abort(FatalError);
            };
            auto ret = gko::share(gko::HipExecutor::create(
                device_id_ % gko::HipExecutor::get_num_devices(), host_exec));
            return ret;
        }
        if (executor_name_ == "omp") {
            if (version.omp_version.tag == not_compiled_tag) {
                FatalErrorInFunction
                    << "OMP Backend was not compiled. Recompile OGL/Ginkgo "
                       "with OMP backend enabled."
                    << abort(FatalError);
            }
            return gko::share(gko::OmpExecutor::create());
        }
        if (executor_name_ == "reference") {
            return host_exec;
        }

        FatalErrorInFunction
            << "OGL does not support the executor: " << executor_name_
            << "\nValid choices are: cuda, hip, dpcpp, omp, or reference"
            << abort(FatalError);
        return {};
    }
};

class ExecutorHandler
    : public PersistentBase<gko::Executor, ExecutorInitFunctor> {
private:
    const bool gko_force_host_buffer_;

    mutable std::shared_ptr<gko::experimental::mpi::communicator> device_comm_;

    const word device_executor_name_;

public:
    ExecutorHandler(const objectRegistry &db, const dictionary &solverControls,
                    const word field_name)
        : PersistentBase<gko::Executor, ExecutorInitFunctor>(
              solverControls.lookupOrDefault("executor", word("reference")) +
                  +"_" + field_name,
              db,
              ExecutorInitFunctor(
                  solverControls.lookupOrDefault("executor", word("reference")),
                  field_name,
                  solverControls.lookupOrDefault("verbose", label(0)),
                  solverControls.lookupOrDefault("ranksPerGPU", label(1)),
                  solverControls.lookupOrDefault("forceHostBuffer", false)),
              true, 0),
          gko_force_host_buffer_(
              solverControls.lookupOrDefault("forceHostBuffer", false)),
          device_comm_(
              (Pstream::parRun())
                  ? std::make_shared<gko::experimental::mpi::communicator>(
                        MPI_COMM_WORLD, gko_force_host_buffer_)
                  : NULL),
          device_executor_name_(
              solverControls.lookupOrDefault("executor", word("reference")))
    {}

    bool get_gko_force_host_buffer() const
    {
        return this->gko_force_host_buffer_;
    }


    const std::shared_ptr<gko::Executor> get_device_exec() const
    {
        return this->get_persistent_object();
    }

    const std::shared_ptr<gko::Executor> get_ref_exec() const
    {
        return get_device_exec()->get_master();
    }

    word get_exec_name() const { return device_executor_name_; }

    std::shared_ptr<gko::experimental::mpi::communicator>
    get_gko_mpi_host_comm() const
    {
        return std::make_shared<gko::experimental::mpi::communicator>(
            MPI_COMM_WORLD, gko_force_host_buffer_);
    }

    std::shared_ptr<gko::experimental::mpi::communicator>
    get_gko_mpi_device_comm() const
    {
        return this->device_comm_;
    }
};

using PersistentExecutor = ExecutorHandler;

}  // namespace Foam
// namespace Foam
#endif
