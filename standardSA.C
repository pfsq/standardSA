/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     | Website:  https://openfoam.org
    \\  /    A nd           | Copyright (C) 2011-2018 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "standardSA.H"
#include "fvOptions.H"
#include "bound.H"
#include "wallDist.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
namespace RASModels
{

// * * * * * * * * * * * * Protected Member Functions  * * * * * * * * * * * //

template<class BasicTurbulenceModel>
tmp<volScalarField> standardSA<BasicTurbulenceModel>::chi() const
{
    return nuTilda_/this->nu();
}


template<class BasicTurbulenceModel>
tmp<volScalarField> standardSA<BasicTurbulenceModel>::fv1
(
    const volScalarField& chi
) const
{
    const volScalarField chi3(pow3(chi));
    return chi3/(chi3 + pow3(Cv1_));
}


template<class BasicTurbulenceModel>
tmp<volScalarField> standardSA<BasicTurbulenceModel>::fv2
(
    const volScalarField& chi,
    const volScalarField& fv1
) const
{
    return 1.0 - chi/(1.0 + chi*fv1);
}


template<class BasicTurbulenceModel>
tmp<volScalarField> standardSA<BasicTurbulenceModel>::ft2
(
    const volScalarField& chi
) const
{
    const volScalarField chi2(pow(chi,2));
    return Ct3_*exp(-1.0*Ct4_*chi2);
}


template<class BasicTurbulenceModel>
tmp<volScalarField> standardSA<BasicTurbulenceModel>::Stilda
(
    const volScalarField& chi,
    const volScalarField& fv1
) const
{
    volScalarField Omega(::sqrt(2.0)*mag(skew(fvc::grad(this->U_))));
    volScalarField Sbar(fv2(chi, fv1)*nuTilda_/sqr(kappa_*y_));

    if (neg_)
    {
        return
        (
            Omega
          + pos(Cv2_*Omega + Sbar)*Sbar
          + neg(Cv2_*Omega + Sbar)*(Omega*(sqr(Cv2_)*Omega + Cv3_*Sbar))
           /((Cv3_ - 2*Cv2_)*Omega - Sbar)
        );
    }
    else
    {
        return
        (
            max
            (
                Omega + Sbar,
                Cs_*Omega
            )
        );
    }
}


template<class BasicTurbulenceModel>
tmp<volScalarField> standardSA<BasicTurbulenceModel>::fw
(
    const volScalarField& Stilda
) const
{
    volScalarField r
    (
        min
        (
            nuTilda_
           /(
               max
               (
                   Stilda,
                   dimensionedScalar(Stilda.dimensions(), small)
               )
              *sqr(kappa_*y_)
            ),
            scalar(10.0)
        )
    );
    r.boundaryFieldRef() == 0.0;

    const volScalarField g(r + Cw2_*(pow6(r) - r));

    return g*pow((1.0 + pow6(Cw3_))/(pow6(g) + pow6(Cw3_)), 1.0/6.0);
}


template<class BasicTurbulenceModel>
void standardSA<BasicTurbulenceModel>::correctNut
(
    const volScalarField& fv1
)
{
    this->nut_ = nuTilda_*fv1;
    if (neg_)
    {
        bound(this->nut_, dimensionedScalar(this->nut_.dimensions(), 0));
    }
    this->nut_.correctBoundaryConditions();
    fv::options::New(this->mesh_).correct(this->nut_);

    BasicTurbulenceModel::correctNut();
}


template<class BasicTurbulenceModel>
void standardSA<BasicTurbulenceModel>::correctNut()
{
    correctNut(fv1(this->chi()));
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

template<class BasicTurbulenceModel>
standardSA<BasicTurbulenceModel>::standardSA
(
    const alphaField& alpha,
    const rhoField& rho,
    const volVectorField& U,
    const surfaceScalarField& alphaRhoPhi,
    const surfaceScalarField& phi,
    const transportModel& transport,
    const word& propertiesName,
    const word& type
)
:
    eddyViscosity<RASModel<BasicTurbulenceModel>>
    (
        type,
        alpha,
        rho,
        U,
        alphaRhoPhi,
        phi,
        transport,
        propertiesName
    ),

    sigmaNut_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "sigmaNut",
            this->coeffDict_,
            0.66666
        )
    ),
    kappa_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "kappa",
            this->coeffDict_,
            0.41
        )
    ),

    Cb1_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Cb1",
            this->coeffDict_,
            0.1355
        )
    ),
    Cb2_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Cb2",
            this->coeffDict_,
            0.622
        )
    ),
    Cw1_(Cb1_/sqr(kappa_) + (1.0 + Cb2_)/sigmaNut_),
    Cw2_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Cw2",
            this->coeffDict_,
            0.3
        )
    ),
    Cw3_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Cw3",
            this->coeffDict_,
            2.0
        )
    ),
    Cv1_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Cv1",
            this->coeffDict_,
            7.1
        )
    ),
    Cv2_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Cv2",
            this->coeffDict_,
            0.7
        )
    ),
    Cv3_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Cv3",
            this->coeffDict_,
            0.9
        )
    ),
    Ct3_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Ct3",
            this->coeffDict_,
            1.2
        )
    ),
    Ct4_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Ct4",
            this->coeffDict_,
            0.5
        )
    ),
    Cn1_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Cn1",
            this->coeffDict_,
            16.0
        )
    ),
    Cs_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Cs",
            this->coeffDict_,
            0.3
        )
    ),

    neg_
    (
        this->coeffDict_.lookupOrDefault("neg", false)
    ),

    nuTilda_
    (
        IOobject
        (
            "nuTilda",
            this->runTime_.timeName(),
            this->mesh_,
            IOobject::MUST_READ,
            IOobject::AUTO_WRITE
        ),
        this->mesh_
    ),

    y_(wallDist::New(this->mesh_).y())
{
    if (type == typeName)
    {
        this->printCoeffs(type);
    }
    if (neg_)
    {
        Info<< "Enabling negative nuTilda" << endl;
    }
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

template<class BasicTurbulenceModel>
bool standardSA<BasicTurbulenceModel>::read()
{
    if (eddyViscosity<RASModel<BasicTurbulenceModel>>::read())
    {
        sigmaNut_.readIfPresent(this->coeffDict());
        kappa_.readIfPresent(this->coeffDict());

        Cb1_.readIfPresent(this->coeffDict());
        Cb2_.readIfPresent(this->coeffDict());
        Cw1_ = Cb1_/sqr(kappa_) + (1.0 + Cb2_)/sigmaNut_;
        Cw2_.readIfPresent(this->coeffDict());
        Cw3_.readIfPresent(this->coeffDict());
        Cv1_.readIfPresent(this->coeffDict());
        Cv2_.readIfPresent(this->coeffDict());
        Cv3_.readIfPresent(this->coeffDict());
        Ct3_.readIfPresent(this->coeffDict());
        Ct4_.readIfPresent(this->coeffDict());
        Cn1_.readIfPresent(this->coeffDict());
        Cs_.readIfPresent(this->coeffDict());

        neg_.readIfPresent
        (
            "neg", this->coeffDict()
        );

        return true;
    }
    else
    {
        return false;
    }
}


template<class BasicTurbulenceModel>
tmp<volScalarField> standardSA<BasicTurbulenceModel>::DnuTildaEff
(
    const volScalarField& chi
) const
{
    volScalarField chi3(pow3(chi));
    volScalarField fn
    (
        pos(chi) + neg(chi)*(Cn1_ + chi3)/(Cn1_ - chi3)
    );


    return volScalarField::New
    (
        "DnuTildaEff",
        (nuTilda_*fn + this->nu())/sigmaNut_
    );
}


template<class BasicTurbulenceModel>
tmp<volScalarField> standardSA<BasicTurbulenceModel>::k() const
{
    return volScalarField::New
    (
        "k",
        this->mesh_,
        dimensionedScalar(dimensionSet(0, 2, -2, 0, 0), 0)
    );
}


template<class BasicTurbulenceModel>
tmp<volScalarField> standardSA<BasicTurbulenceModel>::epsilon() const
{
    WarningInFunction
        << "Turbulence kinetic energy dissipation rate not defined for "
        << "Spalart-Allmaras model. Returning zero field"
        << endl;

    return volScalarField::New
    (
        "epsilon",
        this->mesh_,
        dimensionedScalar(dimensionSet(0, 2, -3, 0, 0), 0)
    );
}


template<class BasicTurbulenceModel>
void standardSA<BasicTurbulenceModel>::correct()
{
    if (!this->turbulence_)
    {
        return;
    }

    // Local references
    const alphaField& alpha = this->alpha_;
    const rhoField& rho = this->rho_;
    const surfaceScalarField& alphaRhoPhi = this->alphaRhoPhi_;
    fv::options& fvOptions(fv::options::New(this->mesh_));

    eddyViscosity<RASModel<BasicTurbulenceModel>>::correct();

    const volScalarField chi(this->chi());
    const volScalarField fv1(this->fv1(chi));

    const volScalarField Stilda(this->Stilda(chi, fv1));
    const volScalarField Omega(::sqrt(2.0)*mag(skew(fvc::grad(this->U_))));


    tmp<fvScalarMatrix> nuTildaEqn
    (
        fvm::ddt(alpha, rho, nuTilda_)
      + fvm::div(alphaRhoPhi, nuTilda_)
      - fvm::laplacian(alpha*rho*DnuTildaEff(chi), nuTilda_)
      - Cb2_/sigmaNut_*alpha*rho*magSqr(fvc::grad(nuTilda_))
     ==
        pos(nuTilda_)
      * (
            Cb1_*(1.0 - ft2(chi))*alpha*rho*Stilda*nuTilda_
          - fvm::Sp((Cw1_*alpha*rho*fw(Stilda)*nuTilda_
          - Cb1_*alpha*rho*ft2(chi)*nuTilda_/sqr(kappa_))/sqr(y_), nuTilda_)
        )
      + neg(nuTilda_)
      * (
            Cb1_*(1.0 - Ct3_)*alpha*rho*Omega*nuTilda_
          + fvm::Sp(Cw1_*alpha*rho*nuTilda_/sqr(y_), nuTilda_)
        )
      + fvOptions(alpha, rho, nuTilda_)
    );

    nuTildaEqn.ref().relax();
    fvOptions.constrain(nuTildaEqn.ref());
    solve(nuTildaEqn);
    fvOptions.correct(nuTilda_);
    if (!neg_)
    {
        bound(nuTilda_, dimensionedScalar(nuTilda_.dimensions(), 0));
    }
    nuTilda_.correctBoundaryConditions();

    correctNut(fv1);
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace RASModels
} // End namespace Foam

// ************************************************************************* //
