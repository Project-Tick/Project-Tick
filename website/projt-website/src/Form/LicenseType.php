<?php

namespace App\Form;

use App\Entity\License;
use App\Entity\LicenseCategory;
use Symfony\Bridge\Doctrine\Form\Type\EntityType;
use Symfony\Component\Form\AbstractType;
use Symfony\Component\Form\Extension\Core\Type\TextareaType;
use Symfony\Component\Form\Extension\Core\Type\TextType;
use Symfony\Component\Form\FormBuilderInterface;
use Symfony\Component\OptionsResolver\OptionsResolver;

class LicenseType extends AbstractType
{
    public function buildForm(FormBuilderInterface $builder, array $options): void
    {
        $builder
            ->add('name', TextType::class, ['attr' => ['class' => 'form-control']])
            ->add('slug', TextType::class, ['attr' => ['class' => 'form-control']])
            ->add('licenseCategory', EntityType::class, [
                'class' => LicenseCategory::class,
                'choice_label' => 'name',
                'attr' => ['class' => 'form-control'],
            ])
            ->add('spdxIdentifier', TextType::class, [
                'required' => false,
                'attr' => ['class' => 'form-control', 'placeholder' => 'e.g. GPL-2.0-only'],
            ])
            ->add('usageDocumentation', TextareaType::class, [
                'required' => false,
                'attr' => ['class' => 'form-control', 'rows' => 5, 'placeholder' => 'Enter suggested usage guidelines here...'],
            ])
            ->add('content', TextareaType::class, [
                'trim' => false,
                'attr' => ['class' => 'form-control', 'rows' => 20, 'style' => 'white-space: pre; font-family: monospace;'],
            ])
        ;
    }

    public function configureOptions(OptionsResolver $resolver): void
    {
        $resolver->setDefaults([
            'data_class' => License::class,
        ]);
    }
}
